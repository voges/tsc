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
 * File format (using e.g. dedicated aux-, nuc-, and qualcodecs):
 * --------------------------------------------------------------
 *   [File header     ]
 *   [SAM header      ]
 *   [Block header 0  ]
 *     [Aux block  0  ]
 *     [Nuc block  0  ]
 *     [Qual block 0  ]
 *   ...
 *   [Block header n-1]
 *     [Aux block  n-1]
 *     [Nuc block  n-1]
 *     [Qual block n-1]
 *   [Block LUT entry 0  ]
 *   ...
 *   [Block LUT entry n-1]
 *
 *
 * File header format:
 * -------------------
 *   unsigned char magic[7]   : "tsc----"
 *   unsigned char version[5] : VERSION
 *   uint64_t      blk_lc     : no. of lines per block
 *   uint64_t      blk_n      : number of blocks written
 *   uint64_t      lut_pos    : beginning of Block LUT
 *
 * SAM header format:
 * ------------------
 *   uint64_t       header_sz : header size
 *   unsigned char* header    : header data
 *
 * Block header format:
 * --------------------
 *   uint64_t blk_cnt         : block count, starting with 0
 *   uint64_t blkl_cnt        : no. of lines in block
 *
 * Block LUT entry format:
 * -----------------------
 *   uint64_t blk_i           : block number
 *   uint64_t pos_min         : smallest position contained in block
 *   uint64_t pos_max         : largest position contained in block
 *   uint64_t offset          : fp offset from beginning of file to block
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

    memset(fileenc->in_sz, 0, sizeof(fileenc->in_sz));
    memset(fileenc->out_sz, 0, sizeof(fileenc->out_sz));
    memset(fileenc->elapsed, 0, sizeof(fileenc->elapsed));
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

static void fileenc_write_file_header(fileenc_t* fileenc)
{
    unsigned char magic[7]   = "tsc----";
    unsigned char version[5] = VERSION;
    uint64_t      blk_lc     = (uint64_t)fileenc->blk_lc;
    uint64_t      blk_n      = (uint64_t)0; /* keep this free for now */
    uint64_t      lut_pos    = (uint64_t)0; /* keep this free for now */

    fileenc->out_sz[OUT_FF] += fwrite_cstr(fileenc->ofp, tsc_prog_name->s);
    fileenc->out_sz[OUT_FF] += fwrite_cstr(fileenc->ofp, "----");
    fileenc->out_sz[OUT_FF] += fwrite_cstr(fileenc->ofp, tsc_version->s);
    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, blk_lc);

    fwrite_uint64(fileenc->ofp, blk_n);
    fwrite_uint64(fileenc->ofp, lut_pos);

    tsc_log("Format: tsc %s\n", VERSION);
    tsc_log("Block size: %zu lines\n", blk_lc);
}

static void fileenc_write_sam_header(fileenc_t* fileenc)
{
    uint64_t       header_sz = (uint64_t)fileenc->samparser->head->len;
    unsigned char* header    = (unsigned char*)fileenc->samparser->head->s;

    fileenc->out_sz[OUT_SH] += fwrite_uint64(fileenc->ofp, header_sz);
    fileenc->out_sz[OUT_SH] += fwrite_buf(fileenc->ofp, header, header_sz);

    tsc_log("Wrote SAM header\n");
}

static void fileenc_write_block_header(fileenc_t* fileenc, const size_t blk_cnt,
                                       const size_t blkl_cnt)
{
    /* Retain information for Block LUT. */
    blklut[blk_cnt][BLKLUT_BLK_I]   = (uint64_t)blk_cnt;
    blklut[blk_cnt][BLKLUT_POS_MIN] = (uint64_t)0;
    blklut[blk_cnt][BLKLUT_POS_MAX] = (uint64_t)0;
    blklut[blk_cnt][BLKLUT_OFFSET]  = (uint64_t)ftell(fileenc->ofp);

    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, (uint64_t)blk_cnt);
    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, (uint64_t)blkl_cnt);
}

static void fileenc_write_block(fileenc_t* fileenc, const size_t blk_cnt,
                                const size_t blkl_cnt)
{
    fileenc_write_block_header(fileenc, blk_cnt, blkl_cnt);

    struct timeval tv0, tv1;

    gettimeofday(&tv0, NULL);
    fileenc->out_sz[OUT_AUX]
        += auxenc_write_block(fileenc->auxenc, fileenc->ofp);
    gettimeofday(&tv1, NULL);
    fileenc->elapsed[ELAPSED_AUXENTR] += tvdiff(tv0, tv1);

    gettimeofday(&tv0, NULL);
    //fileenc->out_sz[OUT_NUC]
        //+= nucenc_write_block(fileenc->nucenc, fileenc->ofp);
    gettimeofday(&tv1, NULL);
    fileenc->elapsed[ELAPSED_NUCENTR] += tvdiff(tv0, tv1);

    gettimeofday(&tv0, NULL);
    //fileenc->out_sz[OUT_QUAL]
        //+= qualenc_write_block(fileenc->qualenc, fileenc->ofp);
    gettimeofday(&tv1, NULL);
    fileenc->elapsed[ELAPSED_QUALENTR] += tvdiff(tv0, tv1);

    tsc_log("Wrote block %zu: %zu line(s)\n", blk_cnt, blkl_cnt);
}

static void fileenc_write_block_lut(fileenc_t* fileenc)
{
    /* Complete the file header. */
    long fpos = ftell(fileenc->ofp);
    fseek(fileenc->ofp, 20, SEEK_SET);
    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, blk_n);
    fileenc->out_sz[OUT_FF] += fwrite_uint64(fileenc->ofp, lut_pos);
    fseek(fileenc->ofp, fpos, SEEK_SET);

    /* Write the LUT. */
    size_t i = 0;
    for (i = 0; i < blk_n; i++) {
        fileenc->out_sz[OUT_FF]
                += fwrite_uint64(fileenc->ofp, blklut[blk_cnt][BLKLUT_BLK_I]);
        fileenc->out_sz[OUT_FF]
                += fwrite_uint64(fileenc->ofp, blklut[blk_cnt][BLKLUT_POS_MIN]);
        fileenc->out_sz[OUT_FF]
                += fwrite_uint64(fileenc->ofp, blklut[blk_cnt][BLKLUT_POS_MAX]);
        fileenc->out_sz[OUT_FF]
                += fwrite_uint64(fileenc->ofp, blklut[blk_cnt][BLKLUT_OFFSET]);
    }

    tsc_log("Wrote block LUT\n");
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

static void fileenc_print_time(fileenc_t* fileenc)
{
    tsc_log("\n"
            "\tTiming Statistics:\n"
            "\t------------------\n"
            "\tTotal time elapsed : %12ld us (%6.2f%%)\n"
            "\t  Predictive Coding\n"
            "\t    AUX            : %12ld us (%6.2f%%)\n"
            "\t    NUC            : %12ld us (%6.2f%%)\n"
            "\t    QUAL           : %12ld us (%6.2f%%)\n"
            "\t  Entropy Coding\n"
            "\t    AUX            : %12ld us (%6.2f%%)\n"
            "\t    NUC            : %12ld us (%6.2f%%)\n"
            "\t    QUAL           : %12ld us (%6.2f%%)\n"
            "\n",
            fileenc->elapsed[ELAPSED_TOTAL],
            (100 * (double)fileenc->elapsed[ELAPSED_TOTAL]
             / (double)fileenc->elapsed[ELAPSED_TOTAL]),
            fileenc->elapsed[ELAPSED_AUXPRED],
            (100 * (double)fileenc->elapsed[ELAPSED_AUXPRED]
             / (double)fileenc->elapsed[ELAPSED_TOTAL]),
            fileenc->elapsed[ELAPSED_NUCPRED],
            (100 * (double)fileenc->elapsed[ELAPSED_NUCPRED]
             / (double)fileenc->elapsed[ELAPSED_TOTAL]),
            fileenc->elapsed[ELAPSED_QUALPRED],
            (100 * (double)fileenc->elapsed[ELAPSED_QUALPRED]
             / (double)fileenc->elapsed[ELAPSED_TOTAL]),
            fileenc->elapsed[ELAPSED_AUXENTR],
            (100 * (double)fileenc->elapsed[ELAPSED_AUXENTR]
             / (double)fileenc->elapsed[ELAPSED_TOTAL]),
            fileenc->elapsed[ELAPSED_NUCENTR],
            (100 * (double)fileenc->elapsed[ELAPSED_NUCENTR]
             / (double)fileenc->elapsed[ELAPSED_TOTAL]),
            fileenc->elapsed[ELAPSED_QUALENTR],
            (100 * (double)fileenc->elapsed[ELAPSED_QUALENTR]
             / (double)fileenc->elapsed[ELAPSED_TOTAL]));
}

void fileenc_encode(fileenc_t* fileenc)
{
    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    size_t blk_cnt  = 0; /* block counter           */
    size_t line_cnt = 0; /* line counter            */
    size_t blkl_cnt = 0; /* block-wise line counter */

    fileenc_write_file_header(fileenc);
    fileenc_write_sam_header(fileenc);

    samrecord_t* samrecord = &(fileenc->samparser->curr);
    while (samparser_next(fileenc->samparser)) {
        if (blkl_cnt >= fileenc->blk_lc) {
            /* Write block. */
            fileenc_write_block(fileenc, blk_cnt, blkl_cnt);
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
        gettimeofday(&tv1, NULL);
        fileenc->elapsed[ELAPSED_AUXPRED] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        nucenc_add_record(fileenc->nucenc,
                          samrecord->int_fields[POS],
                          samrecord->str_fields[CIGAR],
                          samrecord->str_fields[SEQ]);
        gettimeofday(&tv1, NULL);
        fileenc->elapsed[ELAPSED_NUCPRED] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        qualenc_add_record(fileenc->qualenc,
                           samrecord->str_fields[QUAL]);
        gettimeofday(&tv1, NULL);
        fileenc->elapsed[ELAPSED_QUALPRED] += tvdiff(tv0, tv1);

        /* Accumulate input statistics. */
        fileenc->in_sz[IN_QNAME] += strlen(samrecord->str_fields[QNAME]);
        fileenc->in_sz[IN_FLAG]++;
        fileenc->in_sz[IN_RNAME] += strlen(samrecord->str_fields[RNAME]);
        fileenc->in_sz[IN_POS]++;
        fileenc->in_sz[IN_MAPQ]++;
        fileenc->in_sz[IN_CIGAR] += strlen(samrecord->str_fields[CIGAR]);
        fileenc->in_sz[IN_RNEXT] += strlen(samrecord->str_fields[RNEXT]);
        fileenc->in_sz[IN_PNEXT]++;
        fileenc->in_sz[IN_TLEN]++;
        fileenc->in_sz[IN_SEQ] += strlen(samrecord->str_fields[SEQ]);
        fileenc->in_sz[IN_QUAL] += strlen(samrecord->str_fields[QUAL]);
        fileenc->in_sz[IN_OPT] += strlen(samrecord->str_fields[OPT]);

        blkl_cnt++;
        line_cnt++;
    }

    /* Write last block. */
    fileenc_write_block(fileenc, blk_cnt, blkl_cnt);
    blk_cnt++;

    /* Print summary. */
    gettimeofday(&tt1, NULL);
    fileenc->elapsed[ELAPSED_TOTAL] = tvdiff(tt0, tt1);
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
    tsc_log("Took %ld us ~= %.4f s\n", fileenc->elapsed[ELAPSED_TOTAL],
            (double)fileenc->elapsed[ELAPSED_TOTAL] / 1000000);

    /* If selected by the user, print detailed statistics and/or timing info. */
    if (tsc_stats) fileenc_print_stats(fileenc, blk_cnt, line_cnt);
    if (tsc_time) fileenc_print_time(fileenc);
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void filedec_init(filedec_t* filedec, FILE* ifp, FILE* ofp)
{
    filedec->ifp = ifp;
    filedec->ofp = ofp;
    filedec->out_sz = 0;
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

static void filedec_read_file_header(filedec_t* filedec)
{
    unsigned char magic[7];
    unsigned char version[5];
    uint64_t      blk_sz;

    if (fread_buf(filedec->ifp, magic, sizeof(magic)) != sizeof(magic))
        tsc_error("Failed to read magic!\n");
    if (fread_buf(filedec->ifp, version, sizeof(version)) != sizeof(version))
        tsc_error("Failed to read version!\n");
    if (strncmp((const char*)version, (const char*)tsc_version->s,
                sizeof(version)))
        tsc_error("File was compressed with another version: %s\n", version);
    if (fread_uint64(filedec->ifp, &blk_sz) != sizeof(blk_sz))
        tsc_error("Failed to read block size!\n");

    magic[3] = '\0';
    tsc_log("Format: %s %s\n", magic, version);
    tsc_log("Block size: %zu\n", (size_t)blk_sz);
}

static void filedec_read_sam_header(filedec_t* filedec)
{
    uint64_t       header_sz;
    unsigned char* header;

    if (fread_uint64(filedec->ifp, &header_sz) != sizeof(header_sz))
        tsc_error("Failed to read SAM header size!\n");
    header = (unsigned char*)tsc_malloc((size_t)header_sz);
    if (fread_buf(filedec->ifp, header, header_sz) != header_sz)
        tsc_error("Failed to read SAM header!\n");
    filedec->out_sz += fwrite_buf(filedec->ofp, header, header_sz);
    free(header);
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

void filedec_decode(filedec_t* filedec)
{
    struct timeval tv0, tv1;
    gettimeofday(&tv0, NULL);

    size_t line_cnt = 0; /* total no. of lines processed */
    size_t out_sz   = 0; /* total no. of bytes written   */

    filedec_read_file_header(filedec);
    filedec_read_sam_header(filedec);

    /* Block header. */
    uint64_t blk_cnt;
    uint64_t blkl_cnt;

    while (fread_uint64(filedec->ifp, &blk_cnt) == sizeof(blk_cnt)) {
        if (fread_uint64(filedec->ifp, &blkl_cnt) != sizeof(blkl_cnt))
            tsc_error("Failed to read number of lines in block %zu!\n",
                      (size_t)blk_cnt);
        if (tsc_verbose)
            tsc_log("Decoding block %zu: %zu lines\n", (size_t)blk_cnt,
                    (size_t)blkl_cnt);

        /* Allocate memory to prepare decoding of the next block. */
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

        /* Decode block. */
        auxdec_decode_block(filedec->auxdec, filedec->ifp, aux);
        nucdec_decode_block(filedec->nucdec, filedec->ifp, pos, cigar, seq);
        qualdec_decode_block(filedec->qualdec, filedec->ifp, qual);

        /* Write block in correct order to outfile. */
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

    /* Print summary. */
    gettimeofday(&tv1, NULL);
    tsc_log("Decoded %zu line(s) in %zu block(s) and wrote %zu bytes ~= %.2f "
            "GiB\n", line_cnt, (size_t)blk_cnt, out_sz, ((double)out_sz / GB));
    tsc_log("Took %ld us ~= %.4f s\n", tvdiff(tv0, tv1),
            (double)tvdiff(tv0, tv1) / 1000000);

    /* If selected by the user, print detailed statistics. */
    if (tsc_stats)
        filedec_print_stats(filedec, (size_t)blk_cnt, line_cnt, out_sz);
}

void filedec_info(filedec_t* filedec)
{
    tsc_log("Reading block table ...\n");
}

