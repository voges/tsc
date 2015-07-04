/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "filecodec.h"
#include "frw.h"
#include "tsclib.h"
#include "version.h"
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>

/*
 * File format:
 * ------------
 * (Using e.g. dedicated aux-, nuc-, and qualcodecs which generate
 * so-called 'sub-blocks')
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
 *   uint64_t      blk_n      : number of blocks written
 *   uint64_t      lut_pos    : beginning of block LUT
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
 *   Defined by dedicated sub-block codec (e.g. qualcodec)
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

static long tvdiff(struct timeval tv0, struct timeval tv1)
{
    return (tv1.tv_sec - tv0.tv_sec) * 1000000 + tv1.tv_usec - tv0.tv_usec;
}

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void fileenc_init(fileenc_t* fileenc, FILE* ifp, FILE* ofp,
                         const size_t blk_lc)
{
    fileenc->ifp = ifp;
    fileenc->ofp = ofp;
    fileenc->blk_lc = blk_lc;
}

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp, const size_t blk_lc)
{
    fileenc_t* fileenc = (fileenc_t *)tsc_malloc(sizeof(fileenc_t));
    fileenc->samparser = samparser_new(ifp);
    fileenc->auxenc = auxenc_new();
    fileenc->nucenc = nucenc_new();
    fileenc->qualenc = qualenc_new();
    fileenc_init(fileenc, ifp, ofp, blk_lc);
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

static void fileenc_print_stats(fileenc_t* fileenc, const size_t blk_cnt,
                         const size_t line_cnt)
{
    tsc_log("\n"
            "\tCompression Statistics:\n"
            "\t-----------------------\n"
            "\tNumber of blocks written  : %12zu\n"
            "\tNumber of lines processed : %12zu\n"
            "\tBytes written             : %12zu (%6.2f%%)\n"
            "\t  File format             : %12zu (%6.2f%%)\n"
            "\t  SAM header              : %12zu (%6.2f%%)\n"
            "\t  AUX                     : %12zu (%6.2f%%)\n"
            "\t  NUC                     : %12zu (%6.2f%%)\n"
            "\t  QUAL                    : %12zu (%6.2f%%)\n"
            "\tCompression ratios (bytes read / written)\n"
            "\t  NUC                     : %12zu / %12zu (%6.2f%%)\n"
            "\t  QUAL                    : %12zu / %12zu (%6.2f%%)\n"
            "\n",
            blk_cnt,
            line_cnt,
            fileenc->out_sz[OUT_TOTAL],
            (100 * (double)fileenc->out_sz[OUT_TOTAL]
             / (double)fileenc->out_sz[OUT_TOTAL]),
            fileenc->out_sz[OUT_FF],
            (100 * (double)fileenc->out_sz[OUT_FF]
             / (double)fileenc->out_sz[OUT_TOTAL]),
            fileenc->out_sz[OUT_SH],
            (100 * (double)fileenc->out_sz[OUT_SH]
             / (double)fileenc->out_sz[OUT_TOTAL]),
            fileenc->out_sz[OUT_AUX],
            (100 * (double)fileenc->out_sz[OUT_AUX]
             / (double)fileenc->out_sz[OUT_TOTAL]),
            fileenc->out_sz[OUT_NUC],
            (100 * (double)fileenc->out_sz[OUT_NUC]
             / (double)fileenc->out_sz[OUT_TOTAL]),
            fileenc->out_sz[OUT_QUAL],
            (100 * (double)fileenc->out_sz[OUT_QUAL]
             / (double)fileenc->out_sz[OUT_TOTAL]),
            fileenc->in_sz[IN_SEQ], fileenc->out_sz[OUT_NUC],
            (100 * (double)fileenc->in_sz[IN_SEQ]
             / (double)fileenc->out_sz[OUT_NUC]),
            fileenc->in_sz[IN_QUAL], fileenc->out_sz[OUT_QUAL],
            (100 * (double)fileenc->in_sz[IN_QUAL]
             / (double)fileenc->out_sz[OUT_QUAL]));
}

static void fileenc_print_time(long elapsed_total, long elapsed_pred,
                               long elapsed_entr)
{
    tsc_log("\n"
            "\tTiming Statistics:\n"
            "\t------------------\n"
            "\tTotal time elapsed : %12ld us (%6.2f%%)\n"
            "\tPredictive Coding  : %12ld us (%6.2f%%)\n"
            "\tEntropy Coding     : %12ld us (%6.2f%%)\n"
            "\tRemaining          : %12ld us (%6.2f%%)\n"
            "\n",
            elapsed_total, (100*(double)elapsed_total/(double)elapsed_total),
            elapsed_pred , (100*(double)elapsed_pred/(double)elapsed_total),
            elapsed_entr, (100*(double)elapsed_entr/(double)elapsed_total),
            elapsed_total-elapsed_pred-elapsed_entr,
            (100*(double)(elapsed_total-elapsed_pred-elapsed_entr)/
            (double)elapsed_total));
}

void fileenc_encode(fileenc_t* fileenc)
{
    size_t line_cnt = 0;    /* line counter                    */
    long elapsed_total = 0; /* total time used for encoding    */
    long elapsed_pred  = 0; /* time used for predictive coding */
    long elapsed_entr  = 0; /* time used for entropy coding    */

    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    /* Variables for the block header */
    uint64_t blk_cnt  = 0;
    uint64_t blkl_cnt = 0;

    /* LUT */
    uint64_t* lut = NULL;
    size_t lut_itr = 0;

    /* File header */
    unsigned char magic[7]   = "tsc----";
    unsigned char version[5] = VERSION;
    uint64_t      blk_lc     = (uint64_t)fileenc->blk_lc;
    uint64_t      blk_n      = (uint64_t)0; /* keep this free for now */
    uint64_t      lut_pos    = (uint64_t)0; /* keep this free for now */

    fileenc->out_sz[OUT_FF] += fwrite_buf(fileenc->ofp, magic, sizeof(magic));
    fileenc->out_sz[OUT_FF] += fwrite_buf(fileenc->ofp, version, sizeof(version));
    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, blk_lc);
    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, blk_n);
    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, lut_pos);

    tsc_log("Format: tsc %s\n", VERSION);
    tsc_log("Block size: %zu lines\n", blk_lc);

    /* SAM header */
    uint64_t       header_sz = (uint64_t)fileenc->samparser->head->len;
    unsigned char* header    = (unsigned char*)fileenc->samparser->head->s;

    fileenc->out_sz[OUT_SH] += fwrite_uint64(fileenc->ofp, header_sz);
    fileenc->out_sz[OUT_SH] += fwrite_buf(fileenc->ofp, header, header_sz);

    tsc_log("Wrote SAM header\n");

    samrecord_t* samrecord = &(fileenc->samparser->curr);
    while (samparser_next(fileenc->samparser)) {
        if (blkl_cnt >= fileenc->blk_lc) {
            /* Retain information for LUT entry */
            lut = (uint64_t*)tsc_realloc(lut, 4*blk_cnt + 4*sizeof(uint64_t));
            lut[lut_itr++] = blk_cnt;
            lut[lut_itr++] = 0; /* pos_min */
            lut[lut_itr++] = 0; /* pos_max */
            lut[lut_itr++] = (uint64_t)ftell(fileenc->ofp);

            /* Block header */
            fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, blk_cnt);
            fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, blkl_cnt);

            /* Sub-blocks */
            gettimeofday(&tv0, NULL);
            fileenc->out_sz[OUT_AUX]
                += auxenc_write_block(fileenc->auxenc, fileenc->ofp);
            //fileenc->out_sz[OUT_NUC]
                //+= nucenc_write_block(fileenc->nucenc, fileenc->ofp);
            //fileenc->out_sz[OUT_QUAL]
                //+= qualenc_write_block(fileenc->qualenc, fileenc->ofp);
            gettimeofday(&tv1, NULL);
            elapsed_pred += tvdiff(tv0, tv1);

            tsc_log("Wrote block %zu: %zu line(s)\n", blk_cnt, blkl_cnt);

            blk_cnt++;
            blkl_cnt = 0;
        }

        /* Add records to encoders. */
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
        elapsed_entr += tvdiff(tv0, tv1);

        /* Accumulate input statistics. */
        fileenc->in_sz[IN_SEQ] += strlen(samrecord->str_fields[SEQ]);
        fileenc->in_sz[IN_QUAL] += strlen(samrecord->str_fields[QUAL]);

        blkl_cnt++;
        line_cnt++;
    }

    /* Retain information for LUT entry */
    lut = (uint64_t*)tsc_realloc(lut, 4*blk_cnt + 4*sizeof(uint64_t));
    lut[lut_itr++] = blk_cnt;
    lut[lut_itr++] = 0; /* pos_min */
    lut[lut_itr++] = 0; /* pos_max */
    lut[lut_itr++] = (uint64_t)ftell(fileenc->ofp);

    /* Block header (last) */
    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, blk_cnt);
    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, blkl_cnt);

    /* Sub-blocks (last) */
    gettimeofday(&tv0, NULL);
    fileenc->out_sz[OUT_AUX]
        += auxenc_write_block(fileenc->auxenc, fileenc->ofp);
    //fileenc->out_sz[OUT_NUC]
        //+= nucenc_write_block(fileenc->nucenc, fileenc->ofp);
    //fileenc->out_sz[OUT_QUAL]
        //+= qualenc_write_block(fileenc->qualenc, fileenc->ofp);
    gettimeofday(&tv1, NULL);
    elapsed_pred += tvdiff(tv0, tv1);

    tsc_log("Wrote last block %zu: %zu line(s)\n", blk_cnt, blkl_cnt);

    blk_cnt++;

    /* Complete file header with LUT information. */
    lut_pos = (uint64_t)ftell(fileenc->ofp);
    fseek(fileenc->ofp, 20, SEEK_SET);
    fwrite_uint64(fileenc->ofp, blk_cnt);
    fwrite_uint64(fileenc->ofp, lut_pos);
    fseek(fileenc->ofp, 0, SEEK_END);

    /* Block LUT header */
    unsigned char lut_magic[8] = "lut-----";
    uint64_t      lut_sz       = (uint64_t)lut_itr * sizeof(uint64_t);

    fwrite_buf(fileenc->ofp, lut_magic, sizeof(lut_magic));
    fwrite_uint64(fileenc->ofp, lut_sz);

    tsc_log("Wrote block LUT header\n");

    /* Block LUT entries */
    fwrite_buf(fileenc->ofp, (unsigned char*)lut, lut_sz);

    gettimeofday(&tt1, NULL);

    /* Print summary */
    elapsed_total = tvdiff(tt0, tt1);
    fileenc->out_sz[OUT_TOTAL] = fileenc->out_sz[OUT_FF]
                                 + fileenc->out_sz[OUT_SH]
                                 + fileenc->out_sz[OUT_NUC]
                                 + fileenc->out_sz[OUT_QUAL]
                                 + fileenc->out_sz[OUT_AUX];
    tsc_log("Wrote %zu bytes ~= %.2f GiB (%zu line(s) in %zu block(s))\n",
            fileenc->out_sz[OUT_TOTAL],
            ((double)fileenc->out_sz[OUT_TOTAL] / GB),
            line_cnt,
            blk_cnt);
    tsc_log("Took %ld us ~= %.4f s\n", elapsed_total,
            (double)elapsed_total / 1000000);

    /* If selected by the user, print detailed statistics and/or timing info. */
    if (tsc_stats) fileenc_print_stats(fileenc, blk_cnt, line_cnt);
    if (tsc_time) fileenc_print_time(elapsed_total, elapsed_pred, elapsed_entr);
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
                                const size_t line_cnt, const size_t out_sz)
{
    tsc_log("\n"
            "\tDecompression Statistics:\n"
            "\t-------------------------\n"
            "\tNumber of blocks decoded      : %12zu\n"
            "\tSAM records (lines) processed : %12zu\n"
            "\tTotal bytes written           : %12zu\n"
            "\n",
            blk_cnt,
            line_cnt,
            out_sz);
}

static void filedec_print_time(long elapsed_total, long elapsed_dec,
                               long elapsed_wrt)
{
    tsc_log("\n"
            "\tTiming Statistics:\n"
            "\t------------------\n"
            "\tTotal time elapsed : %12ld us (%6.2f%%)\n"
            "\tDecoding           : %12ld us (%6.2f%%)\n"
            "\tWriting            : %12ld us (%6.2f%%)\n"
            "\tRemaining          : %12ld us (%6.2f%%)\n"
            "\n",
            elapsed_total, (100*(double)elapsed_total/(double)elapsed_total),
            elapsed_dec , (100*(double)elapsed_dec/(double)elapsed_total),
            elapsed_wrt, (100*(double)elapsed_wrt/(double)elapsed_total),
            elapsed_total-elapsed_dec-elapsed_wrt,
            (100*(double)(elapsed_total-elapsed_dec-elapsed_wrt)/
            (double)elapsed_total));
}

void filedec_decode(filedec_t* filedec)
{
    size_t line_cnt    = 0; /* total no. of lines processed */
    size_t out_sz      = 0; /* total no. of bytes written   */
    long elapsed_total = 0; /* total time used for encoding */
    long elapsed_dec   = 0; /* time used for decoding       */
    long elapsed_wrt   = 0; /* time used for writing        */

    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    /* Variables for the block header */
    uint64_t blk_cnt;
    uint64_t blkl_cnt;

    /* File header */
    unsigned char magic[7];
    unsigned char version[5];
    uint64_t      blk_lc;
    uint64_t      blk_n;
    uint64_t      lut_pos;

    if (fread_buf(filedec->ifp, magic, sizeof(magic)) != sizeof(magic))
        tsc_error("Failed to read magic!\n");
    if (fread_buf(filedec->ifp, version, sizeof(version)) != sizeof(version))
        tsc_error("Failed to read version!\n");
    if (fread_uint64(filedec->ifp, &blk_lc) != sizeof(blk_lc))
        tsc_error("Failed to read block size!\n");
    if (fread_uint64(filedec->ifp, &blk_n) != sizeof(blk_n))
        tsc_error("Failed to read number of blocks!\n");
    if (fread_uint64(filedec->ifp, &lut_pos) != sizeof(lut_pos))
        tsc_error("Failed to read block LUT position!\n");

    if (strncmp((const char*)version, (const char*)tsc_version->s,
                sizeof(version)))
        tsc_error("File was compressed with another version: %s\n", version);

    magic[3] = '\0';
    tsc_log("Format: %s %s\n", magic, version);
    tsc_log("Block size: %zu\n", (size_t)blk_lc);
    DEBUG("blk_n: %zu", (size_t)blk_n);

    /* SAM header */
    uint64_t       header_sz;
    unsigned char* header;

    if (fread_uint64(filedec->ifp, &header_sz) != sizeof(header_sz))
        tsc_error("Failed to read SAM header size!\n");
    header = (unsigned char*)tsc_malloc((size_t)header_sz);
    if (fread_buf(filedec->ifp, header, header_sz) != header_sz)
        tsc_error("Failed to read SAM header!\n");
    out_sz += fwrite_buf(filedec->ofp, header, header_sz);
    free(header);

    tsc_log("Read SAM header\n");

    size_t i = 0;
    for (i = 0; i < blk_n; i++) {
        /* Block header */
        if (fread_uint64(filedec->ifp, &blk_cnt) != sizeof(blk_cnt))
            tsc_error("Failed to read block count!\n");
        if (fread_uint64(filedec->ifp, &blkl_cnt) != sizeof(blkl_cnt))
            tsc_error("Failed to read number of lines in block %zu!\n",
                      (size_t)blk_cnt);
        if (tsc_verbose)
            tsc_log("Decoding block %zu: %zu lines\n", (size_t)blk_cnt,
                    (size_t)blkl_cnt);

        /* Allocate memory to prepare decoding of the sub-blocks. */
        uint64_t* pos = (uint64_t*)tsc_malloc(sizeof(uint64_t) * blkl_cnt);
        str_t** cigar = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);
        str_t** seq = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);
        str_t** qual = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);
        str_t** aux = (str_t**)tsc_malloc(sizeof(str_t*) * blkl_cnt);

        size_t i = 0;
        for (i = 0; i < blkl_cnt; i++) {
            cigar[i] = str_new();
            seq[i] = str_new();
            qual[i] = str_new();
            aux[i] = str_new();
        }

        /* Decode sub-blocks */
        gettimeofday(&tv0, NULL);
        auxdec_decode_block(filedec->auxdec, filedec->ifp, aux);
        //nucdec_decode_block(filedec->nucdec, filedec->ifp, pos, cigar, seq);
        //qualdec_decode_block(filedec->qualdec, filedec->ifp, qual);
        gettimeofday(&tv1, NULL);
        elapsed_dec += tvdiff(tv0, tv1);

        /* DUMMIES */
        for (i = 0; i < blkl_cnt; i++) {
            str_append_cstr(cigar[i], "cigar");
            str_append_cstr(seq[i], "seq");
            str_append_cstr(qual[i], "qual");
            str_append_cstr(aux[i], "aux");
        }

        /* Write decoded sub-blocks in correct order to outfile. */
        gettimeofday(&tv0, NULL);
        for (i = 0; i < blkl_cnt; i++) {
            enum { QNAME, FLAG, RNAME, POS, MAPQ, CIGAR,
                   RNEXT, PNEXT, TLEN, SEQ, QUAL, OPT };
            char* sam_fields[12];

            char pos_cstr[101]; /* this should be enough space */
            snprintf(pos_cstr, sizeof(pos_cstr), "%"PRIu64, pos[i]);
            sam_fields[POS] = pos_cstr;
            sam_fields[CIGAR] = cigar[i]->s;
            sam_fields[SEQ] = seq[i]->s;
            sam_fields[QUAL] = qual[i]->s;
            char* aux_itr = aux[i]->s;
            sam_fields[QNAME]= aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[FLAG] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[RNAME] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[MAPQ] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[RNEXT] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[PNEXT] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[TLEN] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[OPT] = ++aux_itr;

            /* Write data to file. */
            size_t f = 0;
            for (f = 0; f < 12; f++) {
                out_sz += fwrite_cstr(filedec->ofp, sam_fields[f]);
                out_sz += fwrite_byte(filedec->ofp, '\t');
            }
            out_sz += fwrite_byte(filedec->ofp, '\n');

            /* Free the memory used for the current line. */
            str_free(cigar[i]);
            str_free(seq[i]);
            str_free(qual[i]);
            str_free(aux[i]);
        }

        /* Free memory allocated for this block. */
        free(pos);
        free(cigar);
        free(seq);
        free(qual);
        free(aux);

        line_cnt += blkl_cnt;
        blk_cnt++;
    }
    gettimeofday(&tv1, NULL);
    elapsed_wrt += tvdiff(tv0, tv1);

    gettimeofday(&tt1, NULL);
    elapsed_total += tvdiff(tt0, tt1);

    /* Print summary */
    tsc_log("Decoded %zu line(s) in %zu block(s) and wrote %zu bytes ~= %.2f "
            "GiB\n", line_cnt, (size_t)blk_cnt, out_sz, ((double)out_sz / GB));
    tsc_log("Took %ld us ~= %.4f s\n", elapsed_total,
            (double)elapsed_total / 1000000);

    /* If selected by the user, print detailed statistics. */
    if (tsc_stats)
        filedec_print_stats(filedec, (size_t)blk_cnt, line_cnt, out_sz);
    if (tsc_time)
        filedec_print_time(elapsed_total, elapsed_dec, elapsed_wrt);
}

void filedec_info(filedec_t* filedec)
{
    /* File header */
    unsigned char magic[7];
    unsigned char version[5];
    uint64_t      blk_lc;
    uint64_t      blk_n;
    uint64_t      lut_pos;

    if (fread_buf(filedec->ifp, magic, sizeof(magic)) != sizeof(magic))
        tsc_error("Failed to read magic!\n");
    if (fread_buf(filedec->ifp, version, sizeof(version)) != sizeof(version))
        tsc_error("Failed to read version!\n");
    if (fread_uint64(filedec->ifp, &blk_lc) != sizeof(blk_lc))
        tsc_error("Failed to read block size!\n");
    if (fread_uint64(filedec->ifp, &blk_n) != sizeof(blk_n))
        tsc_error("Failed to read number of blocks!\n");
    DEBUG("blk_n: %zu", blk_n);
    if (fread_uint64(filedec->ifp, &lut_pos) != sizeof(lut_pos))
        tsc_error("Failed to read block LUT position!\n");

    if (strncmp((const char*)version, (const char*)tsc_version->s,
                sizeof(version)))
        tsc_error("File was compressed with another version: %s\n", version);

    /* Seek to LUT position */
    fseek(filedec->ifp, (long)lut_pos, SEEK_SET);
    DEBUG("LUT position: %zu", lut_pos);

    /* Block LUT header */
    unsigned char lut_magic[8];
    uint64_t      lut_sz;

    if (fread_buf(filedec->ifp, lut_magic, sizeof(lut_magic)) != sizeof(lut_magic))
        tsc_error("Failed to read LUT magic!\n");
    DEBUG("%s", lut_magic);
    if (fread_uint64(filedec->ifp, &lut_sz) != sizeof(lut_sz))
        tsc_error("Failed to read LUT size!\n");
    DEBUG("%d", lut_sz);

    if (strncmp((const char*)lut_magic, "lut-----", 8))
        tsc_error("Wrong LUT magic!\n");

    /* Block LUT entries */
    uint64_t* lut = (uint64_t*)tsc_malloc(lut_sz);

    if (fread_buf(filedec->ifp, (unsigned char*)lut, lut_sz) != lut_sz)
        tsc_error("Failed to read LUT entries!\n");

    tsc_log("\n"
            "\tBlock LUT:\n"
            "\t----------\n"
            "\n"
            "\t     blk_cnt       pos_min       pos_max        offset\n");

    size_t lut_itr = 0;
    while (lut_itr < lut_sz/8) {
        printf("\t");
        printf("%12"PRIu64"  ", lut[lut_itr++]);
        printf("%12"PRIu64"  ", lut[lut_itr++]);
        printf("%12"PRIu64"  ", lut[lut_itr++]);
        printf("%12"PRIu64"  ", lut[lut_itr++]);
        printf("\n");
    }
    printf("\n");
}

