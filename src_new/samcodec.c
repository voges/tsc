//
// Copyright (c) 2015 
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
// 

//
// This file is part of tsc.
// 
// Tsc is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Tsc is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with tsc. If not, see <http://www.gnu.org/licenses/>
//

//
// File format:
// ------------
//   * Tsc stores the SAM header as plain ASCII text.
//   * Tsc uses dedicated sub-block codecs which generate 'sub-blocks'.
//   * For fast random-access a LUT (look-up table) containing (amongst others)
//     64-bit offsets to the block headers, is stored at the end of the file.
//     Additionally, to minimize fseek operations, the position of the next
//     block is stored in each block header.
//
//   [File header            ]
//   [SAM header             ]
//   [Block header 0         ]
//     [Sub-block 0.1        ]
//     [Sub-block 0.2        ]
//     ...
//     [Sub-block 0.(M-1)    ]
//   ...
//   [Block header (N-1)     ]
//     [Sub-block (N-1).1    ]
//     [Sub-block (N-1).2    ]
//     ...
//     [Sub-block (N-1).(M-1)]
//   [LUT header             ]
//     [LUT entry 0          ]
//     ...
//     [LUT entry (N-1)      ]
//
// File header format:
// -------------------
//   unsigned char fh_magic[8]   : "tsc----" + '\0'
//   uint8_t       fh_flags      : flags
//   unsigned char fh_version[7] : VERSION + '\0' + '\0'
//   uint64_t      fh_blk_n      : number of blocks
//   uint64_t      fh_sblk_m     : number of sub-blocks per block
//   uint64_t      fh_lut_pos    : LUT position
//
// SAM header format:
// ------------------
//   uint64_t      sh_data_sz : SAM header size
//   unsigned char *sh_data   : SAM header data
//
// Block header format:
// --------------------
//   uint64_t bh_fpos     : fp offset to the beginning of -this- block
//   uint64_t bh_fpos_nxt : fp offset to the beginning of the -next- block
//                          (the last block has all zeros here)
//   uint64_t bh_blk_cnt  : block count, starting with 0
//   uint64_t bh_rec_cnt  : no. of records per block
//   uint64_t bh_chr_cnt  : chromosome number
//   uint64_t bh_pos_min  : smallest position contained in block
//   uint64_t bh_pos_max  : largest position contained in block
//
// Sub-block format:
// -----------------
//   Defined by dedicated sub-block codec
//
// LUT header format:
// ------------------
//   unsigned char lut_magic[8] : "lut----" + '\0'
//   uint64_t      lut_sz       : LUT size
//
// LUT entry format:
// -----------------
//   See block header format
//

#include "samcodec.h"
#include "./frw/frw.h"
#include "tsclib.h"
#include "version.h"
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>

// Indices for SAM statistics
enum { 
    SAM_QNAME,
    SAM_FLAG,
    SAM_RNAME,
    SAM_POS,
    SAM_MAPQ,
    SAM_CIGAR,
    SAM_RNEXT,
    SAM_PNEXT,
    SAM_TLEN,
    SAM_SEQ,
    SAM_QUAL,
    SAM_OPT
};

// Indices for tsc statistics
enum {
    TSC_TOTAL, // total no. of bytes written
    TSC_FF,    // total no. of bytes written for file format
    TSC_SH,    // total no. of bytes written for SAM header
    TSC_AUX,   // total no. of bytes written by auxenc
    TSC_NUC,   // total no. of bytes written by nucenc
    TSC_QUAL   // total no. of bytes written by qualenc
};

static void samenc_init(samenc_t *samenc, FILE *ifp, FILE *ofp)
{
    samenc->ifp = ifp;
    samenc->ofp = ofp;
}

samenc_t * samenc_new(FILE *ifp, FILE *ofp)
{
    samenc_t *samenc = (samenc_t *)tsc_malloc(sizeof(samenc_t));
    samenc->samparser = samparser_new(ifp);
    samenc->auxenc = auxenc_new();
    samenc->nucenc = nucenc_new();
    samenc->qualenc = qualenc_new();
    samenc_init(samenc, ifp, ofp);
    return samenc;
}

void samenc_free(samenc_t *samenc)
{
    if (samenc != NULL) {
        samparser_free(samenc->samparser);
        nucenc_free(samenc->nucenc);
        qualenc_free(samenc->qualenc);
        auxenc_free(samenc->auxenc);
        free(samenc);
        samenc = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void samenc_print_stats(const size_t *sam_sz, 
                               const size_t *tsc_sz,
                               const size_t blk_cnt, 
                               const size_t rec_cnt,
                               const long   et_tot)
{
    size_t sam_total_sz = sam_sz[SAMREC_QNAME]
                        + sam_sz[SAMREC_FLAG]
                        + sam_sz[SAMREC_RNAME]
                        + sam_sz[SAMREC_POS]
                        + sam_sz[SAMREC_MAPQ]
                        + sam_sz[SAMREC_CIGAR]
                        + sam_sz[SAMREC_RNEXT]
                        + sam_sz[SAMREC_PNEXT]
                        + sam_sz[SAMREC_TLEN]
                        + sam_sz[SAMREC_SEQ]
                        + sam_sz[SAMREC_QUAL]
                        + sam_sz[SAMREC_OPT];
    size_t sam_aux_sz = sam_sz[SAMREC_QNAME]
                      + sam_sz[SAMREC_FLAG]
                      + sam_sz[SAMREC_RNAME]
                      + sam_sz[SAMREC_MAPQ]
                      + sam_sz[SAMREC_RNEXT]
                      + sam_sz[SAMREC_PNEXT]
                      + sam_sz[SAMREC_TLEN]
                      + sam_sz[SAMREC_OPT];
    size_t sam_nuc_sz = sam_sz[SAMREC_POS]
                      + sam_sz[SAMREC_CIGAR]
                      + sam_sz[SAMREC_SEQ];
    size_t sam_qual_sz = sam_sz[SAMREC_QUAL];

    tsc_log("\n"
            "\tCompression Statistics:\n"
            "\t-----------------------\n"
            "\tNumber of blocks         : %12zu\n"
            "\tNumber of records        : %12zu\n"
            "\tCompression speed (MiB/s): %12.2f\n"
            "\tTsc file size            : %12zu (%6.2f%%)\n"
            "\t  File format            : %12zu (%6.2f%%)\n"
            "\t  SAM header             : %12zu (%6.2f%%)\n"
            "\t  Aux                    : %12zu (%6.2f%%)\n"
            "\t  Nuc                    : %12zu (%6.2f%%)\n"
            "\t  Qual                   : %12zu (%6.2f%%)\n"
            "\tCompression ratios                 read /      written\n"
            "\t  Total                  : %12zu / %12zu (%6.2f%%)\n"
            "\t  Aux                    : %12zu / %12zu (%6.2f%%)\n"
            "\t  Nuc                    : %12zu / %12zu (%6.2f%%)\n"
            "\t  Qual                   : %12zu / %12zu (%6.2f%%)\n"
            "\n",
            blk_cnt,
            rec_cnt,
            (sam_total_sz / MB) / ((double)et_tot / (double)1000000),
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

static void samenc_print_time(const long et_tot,
                               const long et_pred,
                               const long et_entr,
                               const long et_stat)
{
    tsc_log("\n"
            "\tCompression Timing Statistics:\n"
            "\t------------------------------\n"
            "\tTotal time elapsed  : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\tPredictive Coding   : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\tEntropy Coding      : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\tStatistics          : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\tRemaining (I/O etc.): %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\n",
            et_tot,
            (double)et_tot / (double)1000000,
            (100*(double)et_tot/(double)et_tot),
            et_pred,
            (double)et_pred / (double)1000000,
            (100*(double)et_pred/(double)et_tot),
            et_entr,
            (double)et_entr / (double)1000000,
            (100*(double)et_entr/(double)et_tot),
            et_stat,
            (double)et_stat / (double)1000000,
            (100*(double)et_stat/(double)et_tot),
            et_tot-et_pred-et_entr-et_stat,
            (double)(et_tot-et_pred-et_entr-et_stat)
            / (double)1000000,
            (100*(double)(et_tot-et_pred-et_entr-et_stat)/
            (double)et_tot));
}

void samenc_encode(samenc_t* samenc)
{
    size_t sam_sz[12] = {0}; /* SAM statistics                  */
    size_t tsc_sz[6]  = {0}; /* tsc statistics                  */
    size_t line_cnt = 0;     /* line counter                    */
    uint64_t fpos_prev = 0;  /* fp offset of the previous block */
    long et_tot = 0;  /* total time used for encoding    */
    long et_pred  = 0;  /* time used for predictive coding */
    long et_entr  = 0;  /* time used for entropy coding    */
    long et_stat  = 0;  /* time used for statistics        */

    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    /* File header variables */
    unsigned char fh_magic[6]   = "tsc--"; fh_magic[5] = '\0';
    unsigned char fh_version[6] = VERSION; fh_version[5] = '\0';
    uint64_t      fh_blk_lc     = (uint64_t)FILECODEC_BLK_LC; //=10000LL
    uint64_t      fh_blk_n      = 0; /* keep this free for now */

    /* Write file header */
    tsc_sz[TSC_FF] += fwrite_buf(samenc->ofp, fh_magic, sizeof(fh_magic));
    tsc_sz[TSC_FF] += fwrite_buf(samenc->ofp, fh_version, sizeof(fh_version));
    tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, fh_blk_lc);
    fseek(samenc->ofp, sizeof(fh_blk_n), SEEK_CUR);

    tsc_vlog("Format: tsc %s\n", VERSION);
    tsc_vlog("Block size: %zu lines\n", fh_blk_lc);

    /* SAM header variables */
    uint64_t       sh_sz = (uint64_t)samenc->samparser->head->len;
    unsigned char* sh_data = (unsigned char*)samenc->samparser->head->s;

    /* Write SAM header */
    tsc_sz[TSC_SH] += fwrite_uint64(samenc->ofp, sh_sz);
    tsc_sz[TSC_SH] += fwrite_buf(samenc->ofp, sh_data, sh_sz);

    tsc_vlog("Wrote SAM header\n");

    /* Block header variables */
    uint64_t bh_fpos     = 0;
    uint64_t bh_fpos_nxt = 0;
    uint64_t bh_blk_cnt  = 0;
    uint64_t bh_line_cnt = 0;
    uint64_t bh_chr_cnt  = 0;
    uint64_t bh_pos_min  = 0;
    uint64_t bh_pos_max  = 0;

    samrecord_t* samrecord = &(samenc->samparser->curr);
    while (samparser_next(samenc->samparser)) {
        if (bh_line_cnt >= fh_blk_lc) {
            /* Store the file pointer offset of this block in the previous
             * block header.
             */
            uint64_t fpos_curr = (uint64_t)ftell(samenc->ofp);

            if (bh_blk_cnt > 0) {
                fseek(samenc->ofp, (long)fpos_prev, SEEK_SET);
                fseek(samenc->ofp, (long)sizeof(bh_fpos), SEEK_CUR);
                tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, fpos_curr);
                fseek(samenc->ofp, (long)fpos_curr, SEEK_SET);
            }

            fpos_prev = fpos_curr;

            /* Write block header */
            bh_fpos = (uint64_t)ftell(samenc->ofp);
            tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_fpos);
            fseek(samenc->ofp, (long)sizeof(bh_fpos_nxt), SEEK_CUR);
            tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_blk_cnt);
            tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_line_cnt);
            tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_chr_cnt);
            tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_pos_min);
            tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_pos_max);

            /* Sub-blocks */
            gettimeofday(&tv0, NULL);
            tsc_sz[TSC_AUX]
                += auxenc_write_block(samenc->auxenc, samenc->ofp);
            tsc_sz[TSC_NUC]
                += nucenc_write_block(samenc->nucenc, samenc->ofp);
            tsc_sz[TSC_QUAL]
                += qualenc_write_block(samenc->qualenc, samenc->ofp);
            gettimeofday(&tv1, NULL);
            et_entr += tvdiff(tv0, tv1);

            tsc_vlog("Wrote block %zu: %zu line(s)\n", bh_blk_cnt, bh_line_cnt);

            bh_blk_cnt++;
            bh_line_cnt = 0;
        }

        /* Add records to encoders */
        gettimeofday(&tv0, NULL);

        auxenc_add_record(samenc->auxenc,
                          samrecord->str_fields[QNAME],
                          samrecord->int_fields[FLAG],
                          samrecord->str_fields[RNAME],
                          samrecord->int_fields[MAPQ],
                          samrecord->str_fields[RNEXT],
                          samrecord->int_fields[PNEXT],
                          samrecord->int_fields[TLEN],
                          samrecord->str_fields[OPT]);
        nucenc_add_record(samenc->nucenc,
                          samrecord->int_fields[POS],
                          samrecord->str_fields[CIGAR],
                          samrecord->str_fields[SEQ]);
        qualenc_add_record(samenc->qualenc,
                           samrecord->str_fields[QUAL]);

        gettimeofday(&tv1, NULL);
        et_pred += tvdiff(tv0, tv1);

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
        et_stat += tvdiff(tv0, tv1);

        bh_line_cnt++;
        line_cnt++;
    }

    /* Store the file pointer offset of this block in the previous
     * block header.
     */
    uint64_t fpos_curr = (uint64_t)ftell(samenc->ofp);

    if (bh_blk_cnt > 0) {
        fseek(samenc->ofp, (long)fpos_prev, SEEK_SET);
        fseek(samenc->ofp, (long)sizeof(bh_fpos), SEEK_CUR);
        tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, fpos_curr);
        fseek(samenc->ofp, (long)fpos_curr, SEEK_SET);
    }

    fpos_prev = fpos_curr;

    /* Write block header (last)*/
    bh_fpos = (uint64_t)ftell(samenc->ofp);
    tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_fpos);
    tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, 0); /* last block header */
    tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_blk_cnt);
    tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_line_cnt);
    tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_chr_cnt);
    tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_pos_min);
    tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, bh_pos_max);

    /* Sub-blocks (last) */
    gettimeofday(&tv0, NULL);
    tsc_sz[TSC_AUX]
        += auxenc_write_block(samenc->auxenc, samenc->ofp);
    tsc_sz[TSC_NUC]
        += nucenc_write_block(samenc->nucenc, samenc->ofp);
    tsc_sz[TSC_QUAL]
        += qualenc_write_block(samenc->qualenc, samenc->ofp);
    gettimeofday(&tv1, NULL);
    et_entr += tvdiff(tv0, tv1);

    tsc_vlog("Wrote last block %zu: %zu line(s)\n", bh_blk_cnt, bh_line_cnt);

    fh_blk_n = ++bh_blk_cnt;

    /* Complete file header */
    fseek(samenc->ofp,
          (long)(sizeof(fh_magic)+sizeof(fh_version)+sizeof(fh_blk_lc)),
          SEEK_SET);
    tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, fh_blk_n);
    fseek(samenc->ofp, (long)0, SEEK_END);

    /* Print summary */
    gettimeofday(&tt1, NULL);
    et_tot = tvdiff(tt0, tt1);
    tsc_sz[TSC_TOTAL] = tsc_sz[TSC_FF]
                      + tsc_sz[TSC_SH]
                      + tsc_sz[TSC_NUC]
                      + tsc_sz[TSC_QUAL]
                      + tsc_sz[TSC_AUX];
    tsc_log("Wrote %zu bytes ~= %.2f GiB (%zu line(s) in %zu block(s))\n",
            tsc_sz[TSC_TOTAL], ((double)tsc_sz[TSC_TOTAL] / GB), line_cnt,
            fh_blk_n);
    tsc_log("Took %ld us ~= %.2f s\n", et_tot,
            (double)et_tot / 1000000);

    /* If selected by the user, print detailed statistics and/or timing info. */
    if (tsc_stats)
        samenc_print_stats(sam_sz, tsc_sz, fh_blk_n, line_cnt, et_tot);
    if (tsc_time)
        samenc_print_time(et_tot, et_pred, et_entr,
                           et_stat);
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void samdec_init(samdec_t* samdec, FILE* ifp, FILE* ofp)
{
    samdec->ifp = ifp;
    samdec->ofp = ofp;
}

samdec_t* samdec_new(FILE* ifp, FILE* ofp)
{
    samdec_t* samdec = (samdec_t *)tsc_malloc(sizeof(samdec_t));
    samdec->nucdec = nucdec_new();
    samdec->qualdec = qualdec_new();
    samdec->auxdec = auxdec_new();
    samdec_init(samdec, ifp, ofp);
    return samdec;
}

void samdec_free(samdec_t* samdec)
{
    if (samdec != NULL) {
        nucdec_free(samdec->nucdec);
        qualdec_free(samdec->qualdec);
        auxdec_free(samdec->auxdec);
        free(samdec);
        samdec = NULL;
    } else { /* samdec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void samdec_print_stats(samdec_t* samdec, const size_t blk_cnt,
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

static void samdec_print_time(long et_tot, long elapsed_dec,
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
            et_tot, (100*(double)et_tot/(double)et_tot),
            elapsed_dec , (100*(double)elapsed_dec/(double)et_tot),
            elapsed_wrt, (100*(double)elapsed_wrt/(double)et_tot),
            et_tot-elapsed_dec-elapsed_wrt,
            (100*(double)(et_tot-elapsed_dec-elapsed_wrt)/
            (double)et_tot));
}

void samdec_decode(samdec_t* samdec, str_t* region)
{
    size_t sam_sz      = 0; /* total no. of bytes written   */
    size_t line_cnt    = 0; /* line counter                 */
    long et_tot = 0; /* total time used for encoding */
    long elapsed_dec   = 0; /* time used for decoding       */
    long elapsed_wrt   = 0; /* time used for writing        */

    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    /* File header variables */
    unsigned char fh_magic[6];
    unsigned char fh_version[6];
    uint64_t      fh_blk_lc;
    uint64_t      fh_blk_n;

    /* Read file header */
    if (fread_buf(samdec->ifp, fh_magic, sizeof(fh_magic)) != sizeof(fh_magic))
        tsc_error("Failed to read magic!\n");
    if (fread_buf(samdec->ifp, fh_version, sizeof(fh_version))
            != sizeof(fh_version))
        tsc_error("Failed to read version!\n");
    if (fread_uint64(samdec->ifp, &fh_blk_lc) != sizeof(fh_blk_lc))
        tsc_error("Failed to read no. of lines per block!\n");
    if (fread_uint64(samdec->ifp, &fh_blk_n) != sizeof(fh_blk_n))
        tsc_error("Failed to read number of blocks!\n");

    if (strncmp((const char*)fh_version, (const char*)tsc_version->s,
                sizeof(fh_version)))
        tsc_error("File was compressed with another version: %s\n", fh_version);

    fh_magic[3] = '\0';
    tsc_vlog("Format: %s %s\n", fh_magic, fh_version);
    tsc_vlog("Block size: %"PRIu64"\n", fh_blk_lc);

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

    /* SAM header variables */
    uint64_t       sh_sz;
    unsigned char* sh_data;

    /* Read SAM header */
    if (fread_uint64(samdec->ifp, &sh_sz) != sizeof(sh_sz))
        tsc_error("Failed to read SAM header size!\n");
    sh_data = (unsigned char*)tsc_malloc((size_t)sh_sz);
    if (fread_buf(samdec->ifp, sh_data, sh_sz) != sh_sz)
        tsc_error("Failed to read SAM header!\n");
    sam_sz += fwrite_buf(samdec->ofp, sh_data, sh_sz);
    free(sh_data);

    tsc_vlog("Read SAM header\n");

    /* Block header variables */
    uint64_t bh_fpos;
    uint64_t bh_fpos_nxt;
    uint64_t bh_blk_cnt;
    uint64_t bh_line_cnt;
    uint64_t bh_chr_cnt;
    uint64_t bh_pos_min;
    uint64_t bh_pos_max;

    size_t i = 0;
    for (i = 0; i < fh_blk_n; i++) {
        /* Read block header */
        fread_uint64(samdec->ifp, &bh_fpos);
        fread_uint64(samdec->ifp, &bh_fpos_nxt);
        fread_uint64(samdec->ifp, &bh_blk_cnt);
        fread_uint64(samdec->ifp, &bh_line_cnt);
        fread_uint64(samdec->ifp, &bh_chr_cnt);
        fread_uint64(samdec->ifp, &bh_pos_min);
        fread_uint64(samdec->ifp, &bh_pos_max);

        tsc_vlog("Decoding block %"PRIu64": %"PRIu64" lines\n", bh_blk_cnt,
                 bh_line_cnt);

        /* Allocate memory to prepare decoding of the sub-blocks. */
        str_t**  qname = (str_t**)tsc_malloc(sizeof(str_t*) * bh_line_cnt);
        int64_t* flag  = (int64_t*)tsc_malloc(sizeof(int64_t) * bh_line_cnt);
        str_t**  rname = (str_t**)tsc_malloc(sizeof(str_t*) * bh_line_cnt);
        int64_t* pos   = (int64_t*)tsc_malloc(sizeof(int64_t) * bh_line_cnt);
        int64_t* mapq  = (int64_t*)tsc_malloc(sizeof(int64_t) * bh_line_cnt);
        str_t**  cigar = (str_t**)tsc_malloc(sizeof(str_t*) * bh_line_cnt);
        str_t**  rnext = (str_t**)tsc_malloc(sizeof(str_t*) * bh_line_cnt);
        int64_t* pnext = (int64_t*)tsc_malloc(sizeof(int64_t) * bh_line_cnt);
        int64_t* tlen  = (int64_t*)tsc_malloc(sizeof(int64_t) * bh_line_cnt);
        str_t**  seq   = (str_t**)tsc_malloc(sizeof(str_t*) * bh_line_cnt);
        str_t**  qual  = (str_t**)tsc_malloc(sizeof(str_t*) * bh_line_cnt);
        str_t**  opt   = (str_t**)tsc_malloc(sizeof(str_t*) * bh_line_cnt);

        size_t l = 0;
        for (l = 0; l < bh_line_cnt; l++) {
            qname[l] = str_new();
            rname[l] = str_new();
            cigar[l] = str_new();
            rnext[l] = str_new();
            seq[l]   = str_new();
            qual[l]  = str_new();
            opt[l]   = str_new();
        }

        /* Decode sub-blocks */
        gettimeofday(&tv0, NULL);

        auxdec_decode_block(samdec->auxdec, samdec->ifp, qname, flag, rname,
                            mapq, rnext, pnext, tlen, opt);
        nucdec_decode_block(samdec->nucdec, samdec->ifp, pos, cigar, seq);
        qualdec_decode_block(samdec->qualdec, samdec->ifp, qual);

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

        for (l = 0; l < bh_line_cnt; l++) {
            /* Convert int-fields to C-strings */
            char flag_cstr[101];  /* this should be enough space */
            char pos_cstr[101];   /* this should be enough space */
            char mapq_cstr[101];  /* this should be enough space */
            char pnext_cstr[101]; /* this should be enough space */
            char tlen_cstr[101];  /* this should be enough space */
            snprintf(flag_cstr, sizeof(flag_cstr), "%"PRId64, flag[l]);
            snprintf(pos_cstr, sizeof(pos_cstr), "%"PRId64, pos[l]);
            snprintf(mapq_cstr, sizeof(mapq_cstr), "%"PRId64, mapq[l]);
            snprintf(pnext_cstr, sizeof(pnext_cstr), "%"PRId64, pnext[l]);
            snprintf(tlen_cstr, sizeof(tlen_cstr), "%"PRId64, tlen[l]);

            /* Set pointers to C-strings */
            char* sam_fields[12];
            sam_fields[SAM_QNAME] = qname[l]->s;
            sam_fields[SAM_FLAG]  = flag_cstr;
            sam_fields[SAM_RNAME] = rname[l]->s;
            sam_fields[SAM_POS]   = pos_cstr;
            sam_fields[SAM_MAPQ]  = mapq_cstr;
            sam_fields[SAM_CIGAR] = cigar[l]->s;
            sam_fields[SAM_RNEXT] = rnext[l]->s;
            sam_fields[SAM_PNEXT] = pnext_cstr;
            sam_fields[SAM_TLEN]  = tlen_cstr;
            sam_fields[SAM_SEQ]   = seq[l]->s;
            sam_fields[SAM_QUAL]  = qual[l]->s;
            sam_fields[SAM_OPT]   = opt[l]->s;

            /* Write data to file */
            size_t f = 0;
            for (f = 0; f < 12; f++) {
                sam_sz += fwrite_buf(samdec->ofp,
                                     (unsigned char*)sam_fields[f],
                                     strlen(sam_fields[f]));
                if (f != 11 && strlen(sam_fields[f + 1]))
                    sam_sz += fwrite_byte(samdec->ofp, '\t');
            }
            sam_sz += fwrite_byte(samdec->ofp, '\n');

            /* Free the memory used for the current line. */
            str_free(qname[l]);
            str_free(rname[l]);
            str_free(cigar[l]);
            str_free(rnext[l]);
            str_free(seq[l]);
            str_free(qual[l]);
            str_free(opt[l]);
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

        line_cnt += bh_line_cnt;
    }

    gettimeofday(&tt1, NULL);
    et_tot += tvdiff(tt0, tt1);

    /* Print summary */
    tsc_log("Decoded %zu line(s) in %zu block(s) and wrote %zu bytes ~= "
            "%.2f GiB\n",
             line_cnt, (size_t)fh_blk_n, sam_sz, ((double)sam_sz / GB));
    tsc_log("Took %ld us ~= %.2f s\n", et_tot,
            (double)et_tot / 1000000);

    /* If selected by the user, print detailed statistics. */
    if (tsc_stats)
        samdec_print_stats(samdec, (size_t)fh_blk_n, line_cnt, sam_sz);
    if (tsc_time)
        samdec_print_time(et_tot, elapsed_dec, elapsed_wrt);
}

void samdec_info(samdec_t* samdec)
{
    /* File header */
    unsigned char fh_magic[6];
    unsigned char fh_version[6];
    uint64_t      fh_blk_lc;
    uint64_t      fh_blk_n;

    if (fread_buf(samdec->ifp, fh_magic, sizeof(fh_magic)) != sizeof(fh_magic))
        tsc_error("Failed to read magic!\n");
    if (fread_buf(samdec->ifp, fh_version, sizeof(fh_version))
            != sizeof(fh_version))
        tsc_error("Failed to read version!\n");
    if (fread_uint64(samdec->ifp, &fh_blk_lc) != sizeof(fh_blk_lc))
        tsc_error("Failed to read no. of lines per block!\n");
    if (fread_uint64(samdec->ifp, &fh_blk_n) != sizeof(fh_blk_n))
        tsc_error("Failed to read number of blocks!\n");

    if (strncmp((const char*)fh_version, (const char*)tsc_version->s,
                sizeof(fh_version)))
        tsc_error("File was compressed with another version: %s\n", fh_version);

    fh_magic[3] = '\0';
    tsc_log("Format: %s %s\n", fh_magic, fh_version);
    tsc_log("Block size: %"PRIu64"\n", fh_blk_lc);
    tsc_log("Number of blocks: %"PRIu64"\n", fh_blk_n);

    /* Skip SAM header */
    uint64_t sh_sz = 0;
    if (fread_uint64(samdec->ifp, &sh_sz) != sizeof(sh_sz))
        tsc_error("Failed to read SAM header size!\n");
    fseek(samdec->ifp, (long)sh_sz, SEEK_CUR);

    /* Block header variables */
    uint64_t bh_fpos     = 0;
    uint64_t bh_fpos_nxt = 0;
    uint64_t bh_blk_cnt  = 0;
    uint64_t bh_line_cnt = 0;
    uint64_t bh_chr_cnt  = 0;
    uint64_t bh_pos_min  = 0;
    uint64_t bh_pos_max  = 0;

    tsc_log("\n"
            "\tInfo:\n"
            "\t----\n"
            "\t        fpos      fpos_nxt       blk_cnt      line_cnt"
            "       chr_cnt       pos_min       pos_max\n");

    while (1) {
        fread_uint64(samdec->ifp, &bh_fpos);
        fread_uint64(samdec->ifp, &bh_fpos_nxt);
        fread_uint64(samdec->ifp, &bh_blk_cnt);
        fread_uint64(samdec->ifp, &bh_line_cnt);
        fread_uint64(samdec->ifp, &bh_chr_cnt);
        fread_uint64(samdec->ifp, &bh_pos_min);
        fread_uint64(samdec->ifp, &bh_pos_max);

        printf("\t");
        printf("%12"PRIu64"  ", bh_fpos);
        printf("%12"PRIu64"  ", bh_fpos_nxt);
        printf("%12"PRIu64"  ", bh_blk_cnt);
        printf("%12"PRIu64"  ", bh_line_cnt);
        printf("%12"PRIu64"  ", bh_chr_cnt);
        printf("%12"PRIu64"  ", bh_pos_min);
        printf("%12"PRIu64"  ", bh_pos_max);
        printf("\n");

        if (bh_fpos_nxt)
            fseek(samdec->ifp, (long)bh_fpos_nxt, SEEK_SET);
        else
            break;
    }
    printf("\n");
}

