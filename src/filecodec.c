/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>
 *
 * This file is part of tsc.
 */

/*
 * File format:
 * ------------
 *   * Tsc stores the SAM header as plain ASCII text.
 *   * This example is using dedicated aux-, nuc-, and qualcodecs which generate
 *     so-called 'sub-blocks'.
 *   * For fast random-access a LUT (look-up table) containing (amongst others)
 *     64-bit offsets to the block headers, is stored at the end of the file.
 *
 *
 *   [File header               ]
 *   [SAM header                ]
 *   [Block header 0            ]
 *     [Sub-block 0.1 (aux)     ]
 *     [Sub-block 0.2 (nuc)     ]
 *     [Sub-block 0.3 (qual)    ]
 *   ...
 *   [Block header (n-1)        ]
 *     [Sub-block (n-1).1 (aux) ]
 *     [Sub-block (n-1).2 (nuc) ]
 *     [Sub-block (n-1).3 (qual)]
 *   [LUT header                ]
 *     [LUT entry 0             ]
 *     ...
 *     [LUT entry n-1           ]
 *
 *
 * File header format:
 * -------------------
 *   unsigned char magic[6]   : "tsc--" + '\0'
 *   unsigned char version[6] : VERSION + '\0'
 *   uint64_t      blk_lc     : no. of lines per block
 *   uint64_t      blk_n      : number of blocks
 *   uint64_t      lut_pos    : LUT position
 *
 * SAM header format:
 * ------------------
 *   uint64_t       header_sz : header size
 *   unsigned char* header    : header data
 *
 * Block header format:
 * --------------------
 *   uint64_t blk_cnt  : block count, starting with 0
 *   uint64_t blkl_cnt : no. of lines in block
 *
 * Sub-block format:
 * -----------------
 *   Defined by dedicated sub-block codec (e.g. 'qualcodec')
 *
 * LUT header format:
 * ------------------
 *   unsigned char lut_magic[8] : "lut----" + '\0'
 *   uint64_t      lut_sz       : LUT size
 *
 * LUT entry format:
 * -----------------
 *   uint64_t blk_cnt : block count (identical to blk_cnt in block header)
 *   uint64_t pos_min : smallest position contained in block
 *   uint64_t pos_max : largest position contained in block
 *   uint64_t offset  : fp offset from beginning of file to block
 */

#include "filecodec.h"
#include "./frw/frw.h"
#include "tsclib.h"
#include "version.h"
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>

/* Linear SAM field indices */
enum { SAM_QNAME, SAM_FLAG, SAM_RNAME, SAM_POS, SAM_MAPQ, SAM_CIGAR,
       SAM_RNEXT, SAM_PNEXT, SAM_TLEN, SAM_SEQ, SAM_QUAL, SAM_OPT };

/* Indices for tsc statistics */
enum {
    TSC_TOTAL, /* total no. of bytes written                 */
    TSC_FF,    /* total no. of bytes written for file format */
    TSC_SH,    /* total no. of bytes written for SAM header  */
    TSC_AUX,   /* total no. of bytes written by auxenc       */
    TSC_NUC,   /* total no. of bytes written by nucenc       */
    TSC_QUAL   /* total no. of bytes written by qualenc      */
};

static long tvdiff(struct timeval tv0, struct timeval tv1)
{
    return (tv1.tv_sec - tv0.tv_sec) * 1000000 + tv1.tv_usec - tv0.tv_usec;
}

static size_t ndigits(int64_t x)
{
    /* Ugly, but fast */
    size_t n = 0;
    if (x < 0) n++;
    x = abs(x);

    if (x < 10) return n+1;
    if (x < 100) return n+2;
    if (x < 1000) return n+3;
    if (x < 10000) return n+4;
    if (x < 100000) return n+5;
    if (x < 1000000) return n+6;
    if (x < 10000000) return n+7;
    if (x < 100000000) return n+8;
    if (x < 1000000000) return n+9;
    if (x < 10000000000) return n+10;
    if (x < 100000000000) return n+11;
    if (x < 1000000000000) return n+12;
    if (x < 10000000000000) return n+13;
    if (x < 100000000000000) return n+14;
    if (x < 1000000000000000) return n+15;
    if (x < 10000000000000000) return n+16;
    if (x < 100000000000000000) return n+17;
    if (x < 1000000000000000000) return n+18;
    return n+19; /* INT64_MAX: 2^63 - 1 = 9223372036854775807 */
}

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void fileenc_init(fileenc_t* fileenc, FILE* ifp, FILE* ofp)
{
    fileenc->ifp = ifp;
    fileenc->ofp = ofp;
}

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp)
{
    fileenc_t* fileenc = (fileenc_t *)tsc_malloc(sizeof(fileenc_t));
    fileenc->samparser = samparser_new(ifp);
    fileenc->auxenc = auxenc_new();
    fileenc->nucenc = nucenc_new();
    fileenc->qualenc = qualenc_new();
    fileenc_init(fileenc, ifp, ofp);
    return fileenc;
}

void fileenc_free(fileenc_t* fileenc)
{
    if (fileenc != NULL) {
        samparser_free(fileenc->samparser);
        nucenc_free(fileenc->nucenc);
        qualenc_free(fileenc->qualenc);
        auxenc_free(fileenc->auxenc);
        free(fileenc);
        fileenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void fileenc_print_stats(const size_t* sam_sz, const size_t* tsc_sz,
                                const size_t blk_cnt, const size_t line_cnt)
{
    size_t sam_total_sz = sam_sz[SAM_QNAME]+sam_sz[SAM_FLAG]+sam_sz[SAM_RNAME]
                          +sam_sz[SAM_POS]+sam_sz[SAM_MAPQ]+sam_sz[SAM_CIGAR]
                          +sam_sz[SAM_RNEXT]+sam_sz[SAM_PNEXT]+sam_sz[SAM_TLEN]
                          +sam_sz[SAM_SEQ]+sam_sz[SAM_QUAL]+sam_sz[SAM_OPT];
    size_t sam_aux_sz = sam_sz[SAM_QNAME]+sam_sz[SAM_FLAG]+sam_sz[SAM_RNAME]
                        +sam_sz[SAM_MAPQ]+sam_sz[SAM_RNEXT]+sam_sz[SAM_PNEXT]
                        +sam_sz[SAM_TLEN]+sam_sz[SAM_OPT];
    size_t sam_nuc_sz = sam_sz[SAM_POS]+sam_sz[SAM_CIGAR]+sam_sz[SAM_SEQ];

    tsc_log("\n"
            "\tCompression Statistics:\n"
            "\t-----------------------\n"
            "\tNumber of blocks         : %12zu\n"
            "\tNumber of lines          : %12zu\n"
            "\tNumber of lines per block: %12zu\n"
            "\tTsc file size            : %12zu (%6.2f%%)\n"
            "\t  File format            : %12zu (%6.2f%%)\n"
            "\t  SAM header             : %12zu (%6.2f%%)\n"
            "\t  Aux (everything else)  : %12zu (%6.2f%%)\n"
            "\t  Nuc (pos+cigar+seq)    : %12zu (%6.2f%%)\n"
            "\t  Qual                   : %12zu (%6.2f%%)\n"
            "\tCompression ratios                 read /      written\n"
            "\t  Total                  : %12zu / %12zu (%6.2f%%)\n"
            "\t  Aux (everything else)  : %12zu / %12zu (%6.2f%%)\n"
            "\t  Nuc (pos+cigar+seq)    : %12zu / %12zu (%6.2f%%)\n"
            "\t  Qual                   : %12zu / %12zu (%6.2f%%)\n"
            "\n",
            blk_cnt,
            line_cnt,
            FILECODEC_BLK_LC,
            tsc_sz[TSC_TOTAL],
            (100 * (double)tsc_sz[TSC_TOTAL] / (double)tsc_sz[TSC_TOTAL]),
            tsc_sz[TSC_FF],
            (100 * (double)tsc_sz[TSC_FF] / (double)tsc_sz[TSC_TOTAL]),
            tsc_sz[TSC_SH],
            (100 * (double)tsc_sz[TSC_SH] / (double)tsc_sz[TSC_TOTAL]),
            tsc_sz[TSC_AUX],
            (100 * (double)tsc_sz[TSC_AUX] / (double)tsc_sz[TSC_TOTAL]),
            tsc_sz[TSC_NUC],
            (100 * (double)tsc_sz[TSC_NUC] / (double)tsc_sz[TSC_TOTAL]),
            tsc_sz[TSC_QUAL],
            (100 * (double)tsc_sz[TSC_QUAL] / (double)tsc_sz[TSC_TOTAL]),
            sam_total_sz,
            tsc_sz[TSC_TOTAL],
            (100*(double)tsc_sz[TSC_TOTAL]/(double)sam_total_sz),
            sam_aux_sz,
            tsc_sz[TSC_AUX],
            (100*(double)tsc_sz[TSC_AUX]/(double)sam_aux_sz),
            sam_nuc_sz,
            tsc_sz[TSC_NUC],
            (100*(double)tsc_sz[TSC_NUC]/(double)sam_nuc_sz),
            sam_sz[SAM_QUAL],
            tsc_sz[TSC_QUAL],
            (100*(double)tsc_sz[TSC_QUAL]/(double)sam_sz[SAM_QUAL]));
}

static void fileenc_print_time(long elapsed_total, long elapsed_pred,
                               long elapsed_entr, long elapsed_stat)
{
    tsc_log("\n"
            "\tCompression Timing Statistics:\n"
            "\t------------------------------\n"
            "\tTotal time elapsed          : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\tPredictive Coding           : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\tEntropy Coding              : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\tStatistics                  : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\tRemaining (incl. SAM parser): %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\n",
            elapsed_total,
            (double)elapsed_total / (double)1000000,
            (100*(double)elapsed_total/(double)elapsed_total),
            elapsed_pred,
            (double)elapsed_pred / (double)1000000,
            (100*(double)elapsed_pred/(double)elapsed_total),
            elapsed_entr,
            (double)elapsed_entr / (double)1000000,
            (100*(double)elapsed_entr/(double)elapsed_total),
            elapsed_stat,
            (double)elapsed_stat / (double)1000000,
            (100*(double)elapsed_stat/(double)elapsed_total),
            elapsed_total-elapsed_pred-elapsed_entr-elapsed_stat,
            (double)(elapsed_total-elapsed_pred-elapsed_entr-elapsed_stat)
            / (double)1000000,
            (100*(double)(elapsed_total-elapsed_pred-elapsed_entr-elapsed_stat)/
            (double)elapsed_total));
}

void fileenc_encode(fileenc_t* fileenc)
{
    size_t sam_sz[12] = {0}; /* SAM statistics                  */
    size_t tsc_sz[6]  = {0}; /* tsc statistics                  */
    size_t line_cnt = 0;     /* line counter                    */
    long elapsed_total = 0;  /* total time used for encoding    */
    long elapsed_pred  = 0;  /* time used for predictive coding */
    long elapsed_entr  = 0;  /* time used for entropy coding    */
    long elapsed_stat  = 0;  /* time used for statistics        */

    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    uint64_t blk_cnt  = 0; /* for the block header */
    uint64_t blkl_cnt = 0; /* for the block header */

    uint64_t* lut = (uint64_t*)malloc(0);
    size_t lut_sz = 0; /* for the LUT header */
    size_t lut_itr = 0;

    /* File header */
    unsigned char magic[6]   = "tsc--"; magic[5] = '\0';
    unsigned char version[6] = VERSION; version[5] = '\0';
    uint64_t      blk_lc     = (uint64_t)FILECODEC_BLK_LC;
    uint64_t      blk_n      = (uint64_t)0; /* keep this free for now */
    uint64_t      lut_pos    = (uint64_t)0; /* keep this free for now */

    tsc_sz[TSC_FF] += fwrite_buf(fileenc->ofp, magic, sizeof(magic));
    tsc_sz[TSC_FF] += fwrite_buf(fileenc->ofp, version, sizeof(version));
    tsc_sz[TSC_FF] += fwrite_uint64(fileenc->ofp, blk_lc);
    fseek(fileenc->ofp, sizeof(blk_n) + sizeof(lut_pos), SEEK_CUR);

    tsc_vlog("Format: tsc %s\n", VERSION);
    tsc_vlog("Block size: %zu lines\n", blk_lc);

    /* SAM header */
    uint64_t       header_sz = (uint64_t)fileenc->samparser->head->len;
    unsigned char* header    = (unsigned char*)fileenc->samparser->head->s;

    tsc_sz[TSC_SH] += fwrite_uint64(fileenc->ofp, header_sz);
    tsc_sz[TSC_SH] += fwrite_buf(fileenc->ofp, header, header_sz);

    tsc_vlog("Wrote SAM header\n");

    samrecord_t* samrecord = &(fileenc->samparser->curr);
    while (samparser_next(fileenc->samparser)) {
        if (blkl_cnt >= FILECODEC_BLK_LC) {
            /* Retain information for LUT entry */
            lut_sz += 4 * sizeof(uint64_t);
            lut = (uint64_t*)tsc_realloc(lut, lut_sz);
            lut[lut_itr++] = blk_cnt;
            lut[lut_itr++] = 0; /* pos_min */
            lut[lut_itr++] = 0; /* pos_max */
            lut[lut_itr++] = (uint64_t)ftell(fileenc->ofp);

            /* Block header */
            tsc_sz[TSC_FF] += fwrite_uint64(fileenc->ofp, blk_cnt);
            tsc_sz[TSC_FF] += fwrite_uint64(fileenc->ofp, blkl_cnt);

            /* Sub-blocks */
            gettimeofday(&tv0, NULL);
            tsc_sz[TSC_AUX]
                += auxenc_write_block(fileenc->auxenc, fileenc->ofp);
            tsc_sz[TSC_NUC]
                += nucenc_write_block(fileenc->nucenc, fileenc->ofp);
            tsc_sz[TSC_QUAL]
                += qualenc_write_block(fileenc->qualenc, fileenc->ofp);
            gettimeofday(&tv1, NULL);
            elapsed_entr += tvdiff(tv0, tv1);

            tsc_vlog("Wrote block %zu: %zu line(s)\n", blk_cnt, blkl_cnt);

            blk_cnt++;
            blkl_cnt = 0;
        }

        /* Add records to encoders */
        gettimeofday(&tv0, NULL);

        auxenc_add_record(fileenc->auxenc,
                          samrecord->str_fields[QNAME],
                          samrecord->int_fields[FLAG],
                          samrecord->str_fields[RNAME],
                          samrecord->int_fields[MAPQ],
                          samrecord->str_fields[RNEXT],
                          samrecord->int_fields[PNEXT],
                          samrecord->int_fields[TLEN],
                          samrecord->str_fields[OPT]);
        nucenc_add_record(fileenc->nucenc,
                          samrecord->int_fields[POS],
                          samrecord->str_fields[CIGAR],
                          samrecord->str_fields[SEQ]);
        qualenc_add_record(fileenc->qualenc,
                           samrecord->str_fields[QUAL]);

        gettimeofday(&tv1, NULL);
        elapsed_pred += tvdiff(tv0, tv1);

        /* Accumulate input statistics */
        gettimeofday(&tv0, NULL);

        sam_sz[SAM_QNAME] += strlen(samrecord->str_fields[QNAME]);
        sam_sz[SAM_FLAG]  += ndigits(samrecord->int_fields[FLAG]);
        sam_sz[SAM_RNAME] += strlen(samrecord->str_fields[RNAME]);
        sam_sz[SAM_POS]   += ndigits(samrecord->int_fields[POS]);
        sam_sz[SAM_MAPQ]  += ndigits(samrecord->int_fields[MAPQ]);
        sam_sz[SAM_CIGAR] += strlen(samrecord->str_fields[CIGAR]);
        sam_sz[SAM_RNEXT] += strlen(samrecord->str_fields[RNEXT]);
        sam_sz[SAM_PNEXT] += ndigits(samrecord->int_fields[PNEXT]);
        sam_sz[SAM_TLEN]  += ndigits(samrecord->int_fields[TLEN]);;
        sam_sz[SAM_SEQ]   += strlen(samrecord->str_fields[SEQ]);
        sam_sz[SAM_QUAL]  += strlen(samrecord->str_fields[QUAL]);
        sam_sz[SAM_OPT]   += strlen(samrecord->str_fields[OPT]);

        gettimeofday(&tv1, NULL);
        elapsed_stat += tvdiff(tv0, tv1);

        blkl_cnt++;
        line_cnt++;
    }

    /* Retain information for LUT entry */
    lut_sz += 4 * sizeof(uint64_t);
    lut = (uint64_t*)tsc_realloc(lut, lut_sz);
    lut[lut_itr++] = blk_cnt;
    lut[lut_itr++] = 0; /* pos_min */
    lut[lut_itr++] = 0; /* pos_max */
    lut[lut_itr++] = (uint64_t)ftell(fileenc->ofp);

    /* Block header (last) */
    tsc_sz[TSC_FF] += fwrite_uint64(fileenc->ofp, blk_cnt);
    tsc_sz[TSC_FF] += fwrite_uint64(fileenc->ofp, blkl_cnt);

    /* Sub-blocks (last) */
    gettimeofday(&tv0, NULL);
    tsc_sz[TSC_AUX]
        += auxenc_write_block(fileenc->auxenc, fileenc->ofp);
    tsc_sz[TSC_NUC]
        += nucenc_write_block(fileenc->nucenc, fileenc->ofp);
    tsc_sz[TSC_QUAL]
        += qualenc_write_block(fileenc->qualenc, fileenc->ofp);
    gettimeofday(&tv1, NULL);
    elapsed_entr += tvdiff(tv0, tv1);

    tsc_vlog("Wrote last block %zu: %zu line(s)\n", blk_cnt, blkl_cnt);

    blk_cnt++;

    /* Complete file header with LUT information. */
    lut_pos = (uint64_t)ftell(fileenc->ofp);
    fseek(fileenc->ofp, sizeof(magic)+sizeof(version)+sizeof(blk_lc), SEEK_SET);
    tsc_sz[TSC_FF] += fwrite_uint64(fileenc->ofp, blk_cnt);
    tsc_sz[TSC_FF] += fwrite_uint64(fileenc->ofp, lut_pos);
    fseek(fileenc->ofp, 0, SEEK_END);

    /* LUT header */
    unsigned char lut_magic[8] = "lut----"; lut_magic[7] = '\0';
    //uint64_t      lut_sz       = (uint64_t)lut_itr * sizeof(uint64_t);

    tsc_sz[TSC_FF] += fwrite_buf(fileenc->ofp, lut_magic, sizeof(lut_magic));
    tsc_sz[TSC_FF] += fwrite_uint64(fileenc->ofp, lut_sz);

    tsc_vlog("Wrote LUT header\n");

    /* LUT entries */
    tsc_sz[TSC_FF] += fwrite_buf(fileenc->ofp, (unsigned char*)lut, lut_sz);
    if (!lut) {
        tsc_error("Tried to free NULL pointer!\n");
    } else {
        free(lut);
        lut = NULL;
    }

    tsc_vlog("Wrote LUT entries\n");

    /* Print summary */
    gettimeofday(&tt1, NULL);
    elapsed_total = tvdiff(tt0, tt1);
    tsc_sz[TSC_TOTAL] = tsc_sz[TSC_FF]
                                 + tsc_sz[TSC_SH]
                                 + tsc_sz[TSC_NUC]
                                 + tsc_sz[TSC_QUAL]
                                 + tsc_sz[TSC_AUX];
    tsc_log("Wrote %zu bytes ~= %.2f GiB (%zu line(s) in %zu block(s))\n",
            tsc_sz[TSC_TOTAL], ((double)tsc_sz[TSC_TOTAL] / GB), line_cnt,
            blk_cnt);
    tsc_log("Took %ld us ~= %.2f s\n", elapsed_total,
            (double)elapsed_total / 1000000);

    /* If selected by the user, print detailed statistics and/or timing info. */
    if (tsc_stats)
        fileenc_print_stats(sam_sz, tsc_sz, blk_cnt, line_cnt);
    if (tsc_time)
        fileenc_print_time(elapsed_total, elapsed_pred, elapsed_entr,
                           elapsed_stat);
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void filedec_init(filedec_t* filedec, FILE* ifp, FILE* ofp)
{
    filedec->ifp = ifp;
    filedec->ofp = ofp;
}

filedec_t* filedec_new(FILE* ifp, FILE* ofp)
{
    filedec_t* filedec = (filedec_t *)tsc_malloc(sizeof(filedec_t));
    filedec->nucdec = nucdec_new();
    filedec->qualdec = qualdec_new();
    filedec->auxdec = auxdec_new();
    filedec_init(filedec, ifp, ofp);
    return filedec;
}

void filedec_free(filedec_t* filedec)
{
    if (filedec != NULL) {
        nucdec_free(filedec->nucdec);
        qualdec_free(filedec->qualdec);
        auxdec_free(filedec->auxdec);
        free(filedec);
        filedec = NULL;
    } else { /* filedec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void filedec_print_stats(filedec_t* filedec, const size_t blk_cnt,
                                const size_t line_cnt, const size_t sam_sz)
{
    tsc_log("\n"
            "\tDecompression Statistics:\n"
            "\t-------------------------\n"
            "\tNumber of blocks decoded     : %12zu\n"
            "\tSAM records (lines) processed: %12zu\n"
            "\tTotal bytes written          : %12zu\n"
            "\n",
            blk_cnt, line_cnt, sam_sz);
}

static void filedec_print_time(long elapsed_total, long elapsed_dec,
                               long elapsed_wrt)
{
    tsc_log("\n"
            "\tTiming Statistics:\n"
            "\t------------------\n"
            "\tTotal time elapsed: %12ld us (%6.2f%%)\n"
            "\tDecoding          : %12ld us (%6.2f%%)\n"
            "\tWriting           : %12ld us (%6.2f%%)\n"
            "\tRemaining         : %12ld us (%6.2f%%)\n"
            "\n",
            elapsed_total, (100*(double)elapsed_total/(double)elapsed_total),
            elapsed_dec , (100*(double)elapsed_dec/(double)elapsed_total),
            elapsed_wrt, (100*(double)elapsed_wrt/(double)elapsed_total),
            elapsed_total-elapsed_dec-elapsed_wrt,
            (100*(double)(elapsed_total-elapsed_dec-elapsed_wrt)/
            (double)elapsed_total));
}

void filedec_decode(filedec_t* filedec, str_t* region)
{
    size_t sam_sz      = 0; /* total no. of bytes written   */
    size_t line_cnt    = 0; /* line counter                 */
    long elapsed_total = 0; /* total time used for encoding */
    long elapsed_dec   = 0; /* time used for decoding       */
    long elapsed_wrt   = 0; /* time used for writing        */

    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    uint64_t blk_cnt;  /* for the block header */
    uint64_t blkl_cnt; /* for the block header */

    /* File header */
    unsigned char magic[6];
    unsigned char version[6];
    uint64_t      blk_lc;
    uint64_t      blk_n;
    uint64_t      lut_pos;

    if (fread_buf(filedec->ifp, magic, sizeof(magic)) != sizeof(magic))
        tsc_error("Failed to read magic!\n");
    if (fread_buf(filedec->ifp, version, sizeof(version)) != sizeof(version))
        tsc_error("Failed to read version!\n");
    if (fread_uint64(filedec->ifp, &blk_lc) != sizeof(blk_lc))
        tsc_error("Failed to read no. of lines per block!\n");
    if (fread_uint64(filedec->ifp, &blk_n) != sizeof(blk_n))
        tsc_error("Failed to read number of blocks!\n");
    if (fread_uint64(filedec->ifp, &lut_pos) != sizeof(lut_pos))
        tsc_error("Failed to read LUT position!\n");

    if (strncmp((const char*)version, (const char*)tsc_version->s,
                sizeof(version)))
        tsc_error("File was compressed with another version: %s\n", version);

    magic[3] = '\0';
    tsc_vlog("Format: %s %s\n", magic, version);
    tsc_vlog("Block size: %zu\n", (size_t)blk_lc);

    /* Seek to region */
    uint64_t region_offset = 0;
    if (region) {
        /* TODO: parse region string */
        //region_offset;
        tsc_vlog("Chromosome (rname): \n");
        tsc_vlog("Start: %"PRIu64"\n", 2);
        tsc_vlog("End: %"PRIu64"\n", 3);
        tsc_vlog("Blocks to decompress:\n");
    } else {
        region_offset = 0;
    }

    /* SAM header */
    uint64_t       header_sz;
    unsigned char* header;

    if (fread_uint64(filedec->ifp, &header_sz) != sizeof(header_sz))
        tsc_error("Failed to read SAM header size!\n");
    header = (unsigned char*)tsc_malloc((size_t)header_sz);
    if (fread_buf(filedec->ifp, header, header_sz) != header_sz)
        tsc_error("Failed to read SAM header!\n");
    sam_sz += fwrite_buf(filedec->ofp, header, header_sz);
    free(header);

    tsc_vlog("Read SAM header\n");

    size_t i = 0;
    for (i = 0; i < blk_n; i++) {
        /* Block header */
        if (fread_uint64(filedec->ifp, &blk_cnt) != sizeof(blk_cnt))
            tsc_error("Failed to read block count!\n");
        if (fread_uint64(filedec->ifp, &blkl_cnt) != sizeof(blkl_cnt))
            tsc_error("Failed to read no. of lines in block %zu!\n",
                      (size_t)blk_cnt);

        tsc_vlog("Decoding block %zu: %zu lines\n", (size_t)blk_cnt,
                 (size_t)blkl_cnt);

        /* Allocate memory to prepare decoding of the sub-blocks. */
        str_t**  qname = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);
        int64_t* flag  = (int64_t*)tsc_malloc(sizeof(int64_t) * blkl_cnt);
        str_t**  rname = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);
        int64_t* pos   = (int64_t*)tsc_malloc(sizeof(int64_t) * blkl_cnt);
        int64_t* mapq  = (int64_t*)tsc_malloc(sizeof(int64_t) * blkl_cnt);
        str_t**  cigar = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);
        str_t**  rnext = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);
        int64_t* pnext = (int64_t*)tsc_malloc(sizeof(int64_t) * blkl_cnt);
        int64_t* tlen  = (int64_t*)tsc_malloc(sizeof(int64_t) * blkl_cnt);
        str_t**  seq   = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);
        str_t**  qual  = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);
        str_t**  opt   = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);

        size_t i = 0;
        for (i = 0; i < blkl_cnt; i++) {
            qname[i] = str_new();
            rname[i] = str_new();
            cigar[i] = str_new();
            rnext[i] = str_new();
            seq[i] = str_new();
            qual[i] = str_new();
            opt[i] = str_new();
        }

        /* Decode sub-blocks */
        gettimeofday(&tv0, NULL);

        auxdec_decode_block(filedec->auxdec, filedec->ifp, qname, flag, rname,
                            mapq, rnext, pnext, tlen, opt);
        nucdec_decode_block(filedec->nucdec, filedec->ifp, pos, cigar, seq);
        qualdec_decode_block(filedec->qualdec, filedec->ifp, qual);

        gettimeofday(&tv1, NULL);
        elapsed_dec += tvdiff(tv0, tv1);

        /* These are dummies for testing */
        //for (i = 0; i < blkl_cnt; i++) {
            //str_append_cstr(qname[i], "qname");
            //flag[i] = 2146;
            //str_append_cstr(rname[i], "rname");
            //pos[i] = 905;
            //mapq[i] = 3490;
            //str_append_cstr(cigar[i], "cigar");
            //str_append_cstr(rnext[i], "rnext");
            //pnext[i] = 68307;
            //tlen[i] = 7138;
            //str_append_cstr(seq[i], "seq");
            //str_append_cstr(qual[i], "qual");
            //str_append_cstr(opt[i], "opt");
        //}

        /* Write decoded sub-blocks in correct order to outfile. */
        gettimeofday(&tv0, NULL);

        for (i = 0; i < blkl_cnt; i++) {
            /* Convert int-fields to C-strings */
            char flag_cstr[101];  /* this should be enough space */
            char pos_cstr[101];   /* this should be enough space */
            char mapq_cstr[101];  /* this should be enough space */
            char pnext_cstr[101]; /* this should be enough space */
            char tlen_cstr[101];  /* this should be enough space */
            snprintf(flag_cstr, sizeof(flag_cstr), "%"PRId64, flag[i]);
            snprintf(pos_cstr, sizeof(pos_cstr), "%"PRId64, pos[i]);
            snprintf(mapq_cstr, sizeof(mapq_cstr), "%"PRId64, mapq[i]);
            snprintf(pnext_cstr, sizeof(pnext_cstr), "%"PRId64, pnext[i]);
            snprintf(tlen_cstr, sizeof(tlen_cstr), "%"PRId64, tlen[i]);

            /* Set pointers to C-strings */
            char* sam_fields[12];
            sam_fields[SAM_QNAME] = qname[i]->s;
            sam_fields[SAM_FLAG]  = flag_cstr;
            sam_fields[SAM_RNAME] = rname[i]->s;
            sam_fields[SAM_POS]   = pos_cstr;
            sam_fields[SAM_MAPQ]  = mapq_cstr;
            sam_fields[SAM_CIGAR] = cigar[i]->s;
            sam_fields[SAM_RNEXT] = rnext[i]->s;
            sam_fields[SAM_PNEXT] = pnext_cstr;
            sam_fields[SAM_TLEN]  = tlen_cstr;
            sam_fields[SAM_SEQ]   = seq[i]->s;
            sam_fields[SAM_QUAL]  = qual[i]->s;
            sam_fields[SAM_OPT]   = opt[i]->s;

            /* Write data to file */
            size_t f = 0;
            for (f = 0; f < 12; f++) {
                sam_sz += fwrite_buf(filedec->ofp,
                                     (unsigned char*)sam_fields[f],
                                     strlen(sam_fields[f]));
                if (f != 11 && strlen(sam_fields[f + 1]))
                    sam_sz += fwrite_byte(filedec->ofp, '\t');
            }
            sam_sz += fwrite_byte(filedec->ofp, '\n');

            /* Free the memory used for the current line. */
            str_free(qname[i]);
            str_free(rname[i]);
            str_free(cigar[i]);
            str_free(rnext[i]);
            str_free(seq[i]);
            str_free(qual[i]);
            str_free(opt[i]);
        }

        gettimeofday(&tv1, NULL);
        elapsed_wrt += tvdiff(tv0, tv1);

        /* Free memory allocated for this block. */
        free(qname);
        free(flag);
        free(rname);
        free(pos);
        free(mapq);
        free(cigar);
        free(rnext);
        free(pnext);
        free(tlen);
        free(seq);
        free(qual);
        free(opt);

        line_cnt += blkl_cnt;
        blk_cnt++;
    }

    gettimeofday(&tt1, NULL);
    elapsed_total += tvdiff(tt0, tt1);

    /* Print summary */
    tsc_log("Decoded %zu line(s) in %zu block(s) and wrote %zu bytes ~= "
            "%.2f GiB\n",
             line_cnt, (size_t)blk_cnt, sam_sz, ((double)sam_sz / GB));
    tsc_log("Took %ld us ~= %.2f s\n", elapsed_total,
            (double)elapsed_total / 1000000);

    /* If selected by the user, print detailed statistics. */
    if (tsc_stats)
        filedec_print_stats(filedec, (size_t)blk_cnt, line_cnt, sam_sz);
    if (tsc_time)
        filedec_print_time(elapsed_total, elapsed_dec, elapsed_wrt);
}

void filedec_info(filedec_t* filedec)
{
    /* File header */
    unsigned char magic[6];
    unsigned char version[6];
    uint64_t      blk_lc;
    uint64_t      blk_n;
    uint64_t      lut_pos;

    if (fread_buf(filedec->ifp, magic, sizeof(magic)) != sizeof(magic))
        tsc_error("Failed to read magic!\n");
    if (fread_buf(filedec->ifp, version, sizeof(version)) != sizeof(version))
        tsc_error("Failed to read version!\n");
    if (fread_uint64(filedec->ifp, &blk_lc) != sizeof(blk_lc))
        tsc_error("Failed to read no. of lines per block!\n");
    if (fread_uint64(filedec->ifp, &blk_n) != sizeof(blk_n))
        tsc_error("Failed to read number of blocks!\n");
    if (fread_uint64(filedec->ifp, &lut_pos) != sizeof(lut_pos))
        tsc_error("Failed to read LUT position!\n");

    if (strncmp((const char*)version, (const char*)tsc_version->s,
                sizeof(version)))
        tsc_error("File was compressed with another version: %s\n", version);

    magic[3] = '\0';
    tsc_log("Format: %s %s\n", magic, version);
    tsc_log("Block size: %zu\n", (size_t)blk_lc);
    tsc_log("Number of blocks: %zu\n", (size_t)blk_n);

    /* Seek to LUT position */
    fseek(filedec->ifp, (long)lut_pos, SEEK_SET);

    /* LUT header */
    unsigned char lut_magic[8];
    uint64_t      lut_sz;

    if (fread_buf(filedec->ifp, lut_magic, sizeof(lut_magic))
        != sizeof(lut_magic))
        tsc_error("Failed to read LUT magic!\n");
    if (fread_uint64(filedec->ifp, &lut_sz) != sizeof(lut_sz))
        tsc_error("Failed to read LUT size!\n");

    if (strncmp((const char*)lut_magic, "lut----", 7))
        tsc_error("Wrong LUT magic!\n");

    /* Block LUT entries */
    uint64_t* lut = (uint64_t*)tsc_malloc(lut_sz);

    if (fread_buf(filedec->ifp, (unsigned char*)lut, lut_sz) != lut_sz)
        tsc_error("Failed to read LUT entries!\n");

    tsc_log("\n"
            "\tLUT:\n"
            "\t----\n"
            "\t     blk_cnt       pos_min       pos_max        offset\n");

    size_t lut_itr = 0;
    while (lut_itr < (lut_sz / sizeof(uint64_t))) {
        printf("\t");
        printf("%12"PRIu64"  ", lut[lut_itr++]);
        printf("%12"PRIu64"  ", lut[lut_itr++]);
        printf("%12"PRIu64"  ", lut[lut_itr++]);
        printf("%12"PRIu64"  ", lut[lut_itr++]);
        printf("\n");
    }
    printf("\n");
}

