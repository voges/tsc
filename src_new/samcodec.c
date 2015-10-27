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

#include "samcodec.h"
#include "common.h"
#include "frw/frw.h"
#include "tscfmt.h"
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
        auxenc_free(samenc->auxenc);
        nucenc_free(samenc->nucenc);
        qualenc_free(samenc->qualenc);
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
    size_t tsc_total_sz = tsc_sz[TSC_FF]
                        + tsc_sz[TSC_SH]
                        + tsc_sz[TSC_NUC]
                        + tsc_sz[TSC_QUAL]
                        + tsc_sz[TSC_AUX];

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
            tsc_total_sz,
            (100 * (double)tsc_total_sz / (double)tsc_total_sz),
            tsc_sz[TSC_FF],
            (100 * (double)tsc_sz[TSC_FF] / (double)tsc_total_sz),
            tsc_sz[TSC_SH],
            (100 * (double)tsc_sz[TSC_SH] / (double)tsc_total_sz),
            tsc_sz[TSC_AUX],
            (100 * (double)tsc_sz[TSC_AUX] / (double)tsc_total_sz),
            tsc_sz[TSC_NUC],
            (100 * (double)tsc_sz[TSC_NUC] / (double)tsc_total_sz),
            tsc_sz[TSC_QUAL],
            (100 * (double)tsc_sz[TSC_QUAL] / (double)tsc_total_sz),
            sam_total_sz,
            tsc_total_sz,
            (100*(double)tsc_total_sz/(double)sam_total_sz),
            sam_aux_sz,
            tsc_sz[TSC_AUX],
            (100*(double)tsc_sz[TSC_AUX]/(double)sam_aux_sz),
            sam_nuc_sz,
            tsc_sz[TSC_NUC],
            (100*(double)tsc_sz[TSC_NUC]/(double)sam_nuc_sz),
            sam_sz[SAM_QUAL],
            tsc_sz[TSC_QUAL],
            (100*(double)tsc_sz[TSC_QUAL]/(double)sam_qual_sz));
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

void samenc_encode(samenc_t *samenc)
{
    size_t   sam_sz[12] = {0}; // SAM statistics
    size_t   tsc_sz[5]  = {0}; // Tsc statistics
    uint64_t fpos_prev  = 0;   // fp offset of the previous block
    long     et_tot     = 0;   // Total time used for encoding
    long     et_pred    = 0;   // Time used for predictive coding
    long     et_entr    = 0;   // Time used for entropy coding
    long     et_stat    = 0;   // Time used for statistics

    // Timing
    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    // Set up tsc file header, then seek past it to write it at the end
    tscfh_t tscfh;
    tscfh.magic[0] = 't';
    tscfh.magic[1] = 's';
    tscfh.magic[2] = 'c';
    tscfh.magic[3] = '\0';
    tscfh.flags   = 0x1; // LSB signals SAM
    tscfh.ver[0] = VERSION_MAJ;
    tscfh.ver[1] = '.';
    tscfh.ver[2] = VERSION_MIN;
    tscfh.ver[3] = '\0';
    tscfh.rec_n   = 0;
    tscfh.blk_n   = 0;
    tscfh.sblk_m  = 3; // aux, nux, qual
    fseek(samenc->ofp, sizeof(tscfh), SEEK_SET);

    // Copy SAM header to tsc file
    tscsh_t tscsh;
    tscsh.data_sz = (uint64_t)samenc->samparser->head->len;
    tscsh.data = (unsigned char *)samenc->samparser->head->s;
    tsc_sz[TSC_SH] = tscfmt_write_sh(&tscsh, samenc->ofp);

    // Set up block header
    tscbh_t tscbh;
    tscbh.fpos     = 0;
    tscbh.fpos_nxt = 0;
    tscbh.blk_cnt  = 0;
    tscbh.rec_cnt  = 0;
    tscbh.rec_n    = SAMCODEC_REC_N;
    tscbh.chr_cnt  = 0;
    tscbh.pos_min  = 0;
    tscbh.pos_max  = 0;

    samrec_t *samrec = &(samenc->samparser->curr);
    while (samparser_next(samenc->samparser)) {
        if (tscbh.rec_cnt >= tscbh.rec_n) {
            // Store the file pointer offset of this block in the previous
            // block header
            uint64_t fpos_curr = (uint64_t)ftell(samenc->ofp);
            if (tscbh.blk_cnt > 0) {
                fseek(samenc->ofp, (long)fpos_prev, SEEK_SET);
                fseek(samenc->ofp, (long)sizeof(tscbh.fpos), SEEK_CUR);
                fwrite_uint64(samenc->ofp, fpos_curr); // Don't count this
                fseek(samenc->ofp, (long)fpos_curr, SEEK_SET);
            }
            fpos_prev = fpos_curr;

            // Write block header
            tscbh.fpos = (uint64_t)ftell(samenc->ofp);
            tsc_sz[TSC_FF] += tscfmt_write_bh(&tscbh, samenc->ofp);

            // Write sub-blocks
            gettimeofday(&tv0, NULL);
            tsc_sz[TSC_AUX] +=auxenc_write_block(samenc->auxenc, samenc->ofp);
            tsc_sz[TSC_NUC] +=nucenc_write_block(samenc->nucenc, samenc->ofp);
            tsc_sz[TSC_QUAL]+=qualenc_write_block(samenc->qualenc, samenc->ofp);
            gettimeofday(&tv1, NULL);
            et_entr += tvdiff(tv0, tv1);
            tsc_vlog("Wrote block %zu\n", tscbh.blk_cnt);

            tscbh.blk_cnt++;
            tscbh.rec_cnt = 0;
        }

        // Add record to encoders
        gettimeofday(&tv0, NULL);
        auxenc_add_record(samenc->auxenc,
                          samrec->qname,
                          samrec->flag,
                          samrec->rname,
                          samrec->mapq,
                          samrec->rnext,
                          samrec->pnext,
                          samrec->tlen,
                          samrec->opt);
        nucenc_add_record(samenc->nucenc,
                          samrec->pos,
                          samrec->cigar,
                          samrec->seq);
        qualenc_add_record(samenc->qualenc,
                           samrec->qual);
        gettimeofday(&tv1, NULL);
        et_pred += tvdiff(tv0, tv1);

        // Accumulate input statistics, omit \t's and \n's
        gettimeofday(&tv0, NULL);
        sam_sz[SAM_QNAME] += strlen(samrec->qname);
        sam_sz[SAM_FLAG]  += ndigits(samrec->flag);
        sam_sz[SAM_RNAME] += strlen(samrec->rname);
        sam_sz[SAM_POS]   += ndigits(samrec->pos);
        sam_sz[SAM_MAPQ]  += ndigits(samrec->mapq);
        sam_sz[SAM_CIGAR] += strlen(samrec->cigar);
        sam_sz[SAM_RNEXT] += strlen(samrec->rnext);
        sam_sz[SAM_PNEXT] += ndigits(samrec->pnext);
        sam_sz[SAM_TLEN]  += ndigits(samrec->tlen);
        sam_sz[SAM_SEQ]   += strlen(samrec->seq);
        sam_sz[SAM_QUAL]  += strlen(samrec->qual);
        sam_sz[SAM_OPT]   += strlen(samrec->opt);
        gettimeofday(&tv1, NULL);
        et_stat += tvdiff(tv0, tv1);

        tscbh.rec_cnt++;
        tscfh.rec_n++;
    }

    // Store the file pointer offset of this block in the previous
    // block header
    uint64_t fpos_curr = (uint64_t)ftell(samenc->ofp);
    if (tscbh.blk_cnt > 0) {
        fseek(samenc->ofp, (long)fpos_prev, SEEK_SET);
        fseek(samenc->ofp, (long)sizeof(tscbh.fpos), SEEK_CUR);
        fwrite_uint64(samenc->ofp, fpos_curr); // Don't count this
        fseek(samenc->ofp, (long)fpos_curr, SEEK_SET);
    }
    fpos_prev = fpos_curr;

    // Write -last- block header
    tscbh.fpos = (uint64_t)ftell(samenc->ofp);
    tscbh.fpos_nxt = 0; // Last block header has a zero here
    tsc_sz[TSC_FF] += tscfmt_write_bh(&tscbh, samenc->ofp);

    // Write -last- sub-blocks
    gettimeofday(&tv0, NULL);
    tsc_sz[TSC_AUX]  += auxenc_write_block(samenc->auxenc, samenc->ofp);
    tsc_sz[TSC_NUC]  += nucenc_write_block(samenc->nucenc, samenc->ofp);
    tsc_sz[TSC_QUAL] += qualenc_write_block(samenc->qualenc, samenc->ofp);
    gettimeofday(&tv1, NULL);
    et_entr += tvdiff(tv0, tv1);
    tsc_vlog("Wrote last block %zu\n", tscbh.blk_cnt);

    // Write file header
    tscfh.blk_n = ++tscbh.blk_cnt;
    rewind(samenc->ofp);
    tsc_sz[TSC_FF] = tscfmt_write_fh(&tscfh, samenc->ofp);
    fseek(samenc->ofp, (long)0, SEEK_END);

    // Print summary
    gettimeofday(&tt1, NULL);
    et_tot = tvdiff(tt0, tt1);
    tsc_log("Compressed %zu record(s)\n", tscfh.rec_n);
    tsc_log("Wrote %zu block(s)\n", tscfh.blk_n);
    tsc_log("Took %ld us ~= %.2f s\n", et_tot, (double)et_tot / 1000000);

    // If selected, print detailed statistics and/or timing info
    if (tsc_stats)
        samenc_print_stats(sam_sz, tsc_sz, tscfh.blk_n, tscfh.rec_n, et_tot);
    if (tsc_time)
        samenc_print_time(et_tot, et_pred, et_entr, et_stat);
}

static void samdec_init(samdec_t *samdec, FILE *ifp, FILE *ofp)
{
    samdec->ifp = ifp;
    samdec->ofp = ofp;
}

samdec_t * samdec_new(FILE *ifp, FILE *ofp)
{
    samdec_t *samdec = (samdec_t *)tsc_malloc(sizeof(samdec_t));
    samdec->auxdec = auxdec_new();
    samdec->nucdec = nucdec_new();
    samdec->qualdec = qualdec_new();
    samdec_init(samdec, ifp, ofp);
    return samdec;
}

void samdec_free(samdec_t *samdec)
{
    if (samdec != NULL) {
        auxdec_free(samdec->auxdec);
        nucdec_free(samdec->nucdec);
        qualdec_free(samdec->qualdec);
        free(samdec);
        samdec = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void samdec_print_stats(samdec_t     *samdec,
                               const size_t blk_cnt,
                               const size_t line_cnt,
                               const size_t sam_sz)
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

void samdec_decode(samdec_t *samdec)
{
    size_t sam_sz      = 0; /* total no. of bytes written   */
    size_t line_cnt    = 0; /* line counter                 */
    long et_tot = 0; /* total time used for encoding */
    long elapsed_dec   = 0; /* time used for decoding       */
    long elapsed_wrt   = 0; /* time used for writing        */

    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    // Read and check file header
    tscfh_t tscfh;
    tscfmt_read_fh(&tscfh, samdec->ifp);
    // TODO: check version consistency

    // Skip SAM header
    tscsh_t tscsh;
    tscfmt_read_sh(&tscsh, samdec->ifp);
    fseek(samdec->ifp, (long)(tscsh.data_sz + 1), SEEK_CUR);

    tscbh_t tscbh;

    size_t i = 0;
    for (i = 0; i < tscfh.blk_n; i++) {
        // Read block header
        tscfmt_read_bh(&tscbh, samdec->ifp);
        tsc_vlog("Decoding block %"PRIu64"\n", tscbh.blk_cnt);

        // Allocate memory to prepare decoding of the sub-blocks
        str_t   **qname = (str_t **)tsc_malloc(sizeof(str_t *)*tscbh.rec_cnt);
        int64_t *flag   = (int64_t *)tsc_malloc(sizeof(int64_t)*tscbh.rec_cnt);
        str_t   **rname = (str_t **)tsc_malloc(sizeof(str_t *)*tscbh.rec_cnt);
        int64_t *pos    = (int64_t *)tsc_malloc(sizeof(int64_t)*tscbh.rec_cnt);
        int64_t *mapq   = (int64_t *)tsc_malloc(sizeof(int64_t)*tscbh.rec_cnt);
        str_t   **cigar = (str_t **)tsc_malloc(sizeof(str_t *)*tscbh.rec_cnt);
        str_t   **rnext = (str_t **)tsc_malloc(sizeof(str_t *)*tscbh.rec_cnt);
        int64_t *pnext  = (int64_t *)tsc_malloc(sizeof(int64_t)*tscbh.rec_cnt);
        int64_t *tlen   = (int64_t *)tsc_malloc(sizeof(int64_t)*tscbh.rec_cnt);
        str_t   **seq   = (str_t **)tsc_malloc(sizeof(str_t *)*tscbh.rec_cnt);
        str_t   **qual  = (str_t **)tsc_malloc(sizeof(str_t *)*tscbh.rec_cnt);
        str_t   **opt   = (str_t **)tsc_malloc(sizeof(str_t *)*tscbh.rec_cnt);

        size_t l = 0;
        for (l = 0; l < tscbh.rec_cnt; l++) {
            qname[l] = str_new();
            rname[l] = str_new();
            cigar[l] = str_new();
            rnext[l] = str_new();
            seq[l]   = str_new();
            qual[l]  = str_new();
            opt[l]   = str_new();
        }

        // Decode sub-blocks
        gettimeofday(&tv0, NULL);

        auxdec_decode_block(samdec->auxdec,
                            samdec->ifp,
                            qname,
                            flag,
                            rname,
                            mapq,
                            rnext,
                            pnext,
                            tlen,
                            opt);
        nucdec_decode_block(samdec->nucdec,
                            samdec->ifp,
                            pos,
                            cigar,
                            seq);
        qualdec_decode_block(samdec->qualdec,
                             samdec->ifp,
                             qual);

        gettimeofday(&tv1, NULL);
        elapsed_dec += tvdiff(tv0, tv1);

        // These are dummies for testing
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

        // Write decoded sub-blocks in correct order to outfile
        gettimeofday(&tv0, NULL);

        for (l = 0; l < tscbh.rec_cnt; l++) {
            // Convert int-fields to C-strings (101 bytes should be enough)
            char flag_cstr[101];
            char pos_cstr[101];
            char mapq_cstr[101];
            char pnext_cstr[101];
            char tlen_cstr[101];

            snprintf(flag_cstr, sizeof(flag_cstr), "%"PRId64, flag[l]);
            snprintf(pos_cstr, sizeof(pos_cstr), "%"PRId64, pos[l]);
            snprintf(mapq_cstr, sizeof(mapq_cstr), "%"PRId64, mapq[l]);
            snprintf(pnext_cstr, sizeof(pnext_cstr), "%"PRId64, pnext[l]);
            snprintf(tlen_cstr, sizeof(tlen_cstr), "%"PRId64, tlen[l]);

            char *sam_fields[12];
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

            // Write data to file
            size_t f = 0;
            for (f = 0; f < 12; f++) {
                sam_sz += fwrite_buf(samdec->ofp,
                                     (unsigned char*)sam_fields[f],
                                     strlen(sam_fields[f]));
                if (f != 11 && strlen(sam_fields[f + 1]))
                    sam_sz += fwrite_byte(samdec->ofp, '\t');
            }
            sam_sz += fwrite_byte(samdec->ofp, '\n');

            // Free the memory used for the current line
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

        // Free memory allocated for this block
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

        line_cnt += tscbh.rec_cnt;
    }

    gettimeofday(&tt1, NULL);
    et_tot += tvdiff(tt0, tt1);

    // Print summary
    tsc_log("Decoded %zu line(s) in %zu block(s) and wrote %zu bytes ~= "
            "%.2f GiB\n",
             line_cnt,
            (size_t)tscfh.blk_n,
            sam_sz,
            ((double)sam_sz / GB));
    tsc_log("Took %ld us ~= %.2f s\n", et_tot, (double)et_tot / 1000000);

    // If selected by the user, print detailed statistics
    if (tsc_stats)
        samdec_print_stats(samdec, tscfh.blk_n, line_cnt, sam_sz);
    if (tsc_time)
        samdec_print_time(et_tot, elapsed_dec, elapsed_wrt);
}

void samdec_info(samdec_t *samdec)
{
    // Read and check file header
    tscfh_t tscfh;
    tscfmt_read_fh(&tscfh, samdec->ifp);
    // TODO: check version consistency

    // Skip SAM header
    tscsh_t tscsh;
    tscfmt_read_sh(&tscsh, samdec->ifp);
    fseek(samdec->ifp, (long)(tscsh.data_sz + 1), SEEK_CUR);

    // Read and print block headers
    tscbh_t tscbh;

    tsc_log("\n"
            "\tInfo:\n"
            "\t----\n"
            "\t        fpos      fpos_nxt       blk_cnt       rec_cnt"
            "         rec_n       chr_cnt       pos_min       pos_max\n");

    while (1) {
        tscfmt_read_bh(&tscbh, samdec->ifp);

        printf("\t");
        printf("%12"PRIu64"  ", tscbh.fpos);
        printf("%12"PRIu64"  ", tscbh.fpos_nxt);
        printf("%12"PRIu64"  ", tscbh.blk_cnt);
        printf("%12"PRIu64"  ", tscbh.rec_cnt);
        printf("%12"PRIu64"  ", tscbh.rec_n);
        printf("%12"PRIu64"  ", tscbh.chr_cnt);
        printf("%12"PRIu64"  ", tscbh.pos_min);
        printf("%12"PRIu64"  ", tscbh.pos_max);
        printf("\n");

        if (tscbh.fpos_nxt)
            fseek(samdec->ifp, (long)tscbh.fpos_nxt, SEEK_SET);
        else
            break; // Last block has zeros here
    }
    printf("\n");
}

