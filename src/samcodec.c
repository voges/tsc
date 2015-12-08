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
#include "tscfmt.h"
#include "tsclib.h"
#include "tvclib/frw.h"
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
    SAM_OPT,
    SAM_CTRL, // Count \t's and \n's here
    SAM_HEAD  // SAM header
};

// Indices for tsc statistics
enum {
    TSC_FH,     // File header
    TSC_SH,     // SAM header
    TSC_BH,     // Block header(s)
    TSC_AUX,    // Total no. of bytes written by auxenc
    TSC_ID,     // Total no. of bytes written by idenc
    TSC_NUC,    // Total no. of bytes written by nucenc
    TSC_PAIR,   // Total no. of bytes written by pairenc
    TSC_QUAL    // Total no. of bytes written by qualenc
};

// Indices for timing statistics
enum {
    ET_TOT, // Total time elapsed
    ET_AUX,
    ET_ID,
    ET_NUC,
    ET_PAIR,
    ET_QUAL,
    ET_REM  // Remaining (I/O, statistics, etc.)
};

static void samenc_init(samenc_t *samenc,
                        FILE *ifp,
                        FILE *ofp,
                        unsigned int blk_sz)
{
    samenc->ifp = ifp;
    samenc->ofp = ofp;
    samenc->blk_sz = blk_sz;
}

samenc_t * samenc_new(FILE *ifp, FILE *ofp, unsigned int blk_sz)
{
    samenc_t *samenc = (samenc_t *)tsc_malloc(sizeof(samenc_t));
    samenc->samparser = samparser_new(ifp);
    samenc->auxenc = auxenc_new();
    samenc->idenc = idenc_new();
    samenc->nucenc = nucenc_new();
    samenc->pairenc = pairenc_new();
    samenc->qualenc = qualenc_new();
    samenc_init(samenc, ifp, ofp, blk_sz);
    return samenc;
}

void samenc_free(samenc_t *samenc)
{
    if (samenc != NULL) {
        samparser_free(samenc->samparser);
        auxenc_free(samenc->auxenc);
        idenc_free(samenc->idenc);
        nucenc_free(samenc->nucenc);
        pairenc_free(samenc->pairenc);
        qualenc_free(samenc->qualenc);
        free(samenc);
        samenc = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void samenc_print_stats(const size_t  *sam_sz,
                               const size_t  *tsc_sz,
                               const tscfh_t *tscfh,
                               const long    *et)
{
    size_t sam_total_sz = sam_sz[SAM_QNAME]
                        + sam_sz[SAM_FLAG]
                        + sam_sz[SAM_RNAME]
                        + sam_sz[SAM_POS]
                        + sam_sz[SAM_MAPQ]
                        + sam_sz[SAM_CIGAR]
                        + sam_sz[SAM_RNEXT]
                        + sam_sz[SAM_PNEXT]
                        + sam_sz[SAM_TLEN]
                        + sam_sz[SAM_SEQ]
                        + sam_sz[SAM_QUAL]
                        + sam_sz[SAM_OPT]
                        + sam_sz[SAM_CTRL]  // \t's and \n's
                        + sam_sz[SAM_HEAD]; // Don't forget the SAM header
    size_t sam_aux_sz = sam_sz[SAM_FLAG]
                      + sam_sz[SAM_MAPQ]
                      + sam_sz[SAM_OPT];
    size_t sam_id_sz = sam_sz[SAM_QNAME];
    size_t sam_nuc_sz = sam_sz[SAM_RNAME]
                      + sam_sz[SAM_POS]
                      + sam_sz[SAM_CIGAR]
                      + sam_sz[SAM_SEQ];
    size_t sam_pair_sz = sam_sz[SAM_RNEXT]
                         + sam_sz[SAM_PNEXT]
                         + sam_sz[SAM_TLEN];
    size_t sam_qual_sz = sam_sz[SAM_QUAL];
    size_t tsc_total_sz = tsc_sz[TSC_FH]
                        + tsc_sz[TSC_SH]
                        + tsc_sz[TSC_BH]
                        + tsc_sz[TSC_AUX]
                        + tsc_sz[TSC_ID]
                        + tsc_sz[TSC_NUC]
                        + tsc_sz[TSC_PAIR]
                        + tsc_sz[TSC_QUAL];

    tsc_log("\n"
            "\tStatistics:\n"
            "\t-----------\n"
            "\tNumber of records   : %12zu\n"
            "\tNumber of blocks    : %12zu\n"
            "\n"
            "\tSAM file size       : %12zu (%6.2f%%)\n"
            "\t  QNAME             : %12zu (%6.2f%%)\n"
            "\t  FLAG              : %12zu (%6.2f%%)\n"
            "\t  RNAME             : %12zu (%6.2f%%)\n"
            "\t  POS               : %12zu (%6.2f%%)\n"
            "\t  MAPQ              : %12zu (%6.2f%%)\n"
            "\t  CIGAR             : %12zu (%6.2f%%)\n"
            "\t  RNEXT             : %12zu (%6.2f%%)\n"
            "\t  PNEXT             : %12zu (%6.2f%%)\n"
            "\t  TLEN              : %12zu (%6.2f%%)\n"
            "\t  SEQ               : %12zu (%6.2f%%)\n"
            "\t  QUAL              : %12zu (%6.2f%%)\n"
            "\t  OPT               : %12zu (%6.2f%%)\n"
            "\t  CTRL (\\t, \\n)     : %12zu (%6.2f%%)\n"
            "\t  HEAD (SAM header) : %12zu (%6.2f%%)\n"
            "\n"
            "\tTsc file size       : %12zu (%6.2f%%)\n"
            "\t  File header       : %12zu (%6.2f%%)\n"
            "\t  SAM header        : %12zu (%6.2f%%)\n"
            "\t  Block header(s)   : %12zu (%6.2f%%)\n"
            "\t  Aux               : %12zu (%6.2f%%)\n"
            "\t  Id                : %12zu (%6.2f%%)\n"
            "\t  Nuc               : %12zu (%6.2f%%)\n"
            "\t  Pair              : %12zu (%6.2f%%)\n"
            "\t  Qual              : %12zu (%6.2f%%)\n"
            "\n"
            "\tCompression ratios             SAM /          tsc\n"
            "\t  Total             : %12zu / %12zu (%6.2f%%)\n"
            "\t  Aux               : %12zu / %12zu (%6.2f%%)\n"
            "\t  Id                : %12zu / %12zu (%6.2f%%)\n"
            "\t  Nuc               : %12zu / %12zu (%6.2f%%)\n"
            "\t  Pair              : %12zu / %12zu (%6.2f%%)\n"
            "\t  Qual              : %12zu / %12zu (%6.2f%%)\n"
            "\n"
            "\tTiming\n"
            "\t  Total time elapsed: %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\t  Aux               : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\t  Id                : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\t  Nuc               : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\t  Pair              : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\t  Qual              : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\t  Remaining         : %12ld us ~= %12.2f s (%6.2f%%)\n"
            "\n"
            "\tSpeed\n"
            "\t  Total             : %12.2f MB/s\n"
            "\t  Aux               : %12.2f MB/s\n"
            "\t  Id                : %12.2f MB/s\n"
            "\t  Nuc               : %12.2f MB/s\n"
            "\t  Pair              : %12.2f MB/s\n"
            "\t  Qual              : %12.2f MB/s\n"
            "\n",
            tscfh->rec_n,
            tscfh->blk_n,

            sam_total_sz,
            (100 * (double)sam_total_sz / (double)sam_total_sz),
            sam_sz[SAM_QNAME],
            (100 * (double)sam_sz[SAM_QNAME] / (double)sam_total_sz),
            sam_sz[SAM_FLAG],
            (100 * (double)sam_sz[SAM_FLAG] / (double)sam_total_sz),
            sam_sz[SAM_RNAME],
            (100 * (double)sam_sz[SAM_RNAME] / (double)sam_total_sz),
            sam_sz[SAM_POS],
            (100 * (double)sam_sz[SAM_POS] / (double)sam_total_sz),
            sam_sz[SAM_MAPQ],
            (100 * (double)sam_sz[SAM_MAPQ] / (double)sam_total_sz),
            sam_sz[SAM_CIGAR],
            (100 * (double)sam_sz[SAM_CIGAR] / (double)sam_total_sz),
            sam_sz[SAM_RNEXT],
            (100 * (double)sam_sz[SAM_RNEXT] / (double)sam_total_sz),
            sam_sz[SAM_PNEXT],
            (100 * (double)sam_sz[SAM_PNEXT] / (double)sam_total_sz),
            sam_sz[SAM_TLEN],
            (100 * (double)sam_sz[SAM_TLEN] / (double)sam_total_sz),
            sam_sz[SAM_SEQ],
            (100 * (double)sam_sz[SAM_SEQ] / (double)sam_total_sz),
            sam_sz[SAM_QUAL],
            (100 * (double)sam_sz[SAM_QUAL] / (double)sam_total_sz),
            sam_sz[SAM_OPT],
            (100 * (double)sam_sz[SAM_OPT] / (double)sam_total_sz),
            sam_sz[SAM_CTRL],
            (100 * (double)sam_sz[SAM_CTRL] / (double)sam_total_sz),
            sam_sz[SAM_HEAD],
            (100 * (double)sam_sz[SAM_HEAD] / (double)sam_total_sz),

            tsc_total_sz,
            (100 * (double)tsc_total_sz / (double)tsc_total_sz),
            tsc_sz[TSC_FH],
            (100 * (double)tsc_sz[TSC_FH] / (double)tsc_total_sz),
            tsc_sz[TSC_SH],
            (100 * (double)tsc_sz[TSC_SH] / (double)tsc_total_sz),
            tsc_sz[TSC_BH],
            (100 * (double)tsc_sz[TSC_BH] / (double)tsc_total_sz),
            tsc_sz[TSC_AUX],
            (100 * (double)tsc_sz[TSC_AUX] / (double)tsc_total_sz),
            tsc_sz[TSC_ID],
            (100 * (double)tsc_sz[TSC_ID] / (double)tsc_total_sz),
            tsc_sz[TSC_NUC],
            (100 * (double)tsc_sz[TSC_NUC] / (double)tsc_total_sz),
            tsc_sz[TSC_PAIR],
            (100 * (double)tsc_sz[TSC_PAIR] / (double)tsc_total_sz),
            tsc_sz[TSC_QUAL],
            (100 * (double)tsc_sz[TSC_QUAL] / (double)tsc_total_sz),

            sam_total_sz,
            tsc_total_sz,
            (100*(double)tsc_total_sz/(double)sam_total_sz),
            sam_aux_sz,
            tsc_sz[TSC_AUX],
            (100*(double)tsc_sz[TSC_AUX]/(double)sam_aux_sz),
            sam_id_sz,
            tsc_sz[TSC_ID],
            (100*(double)tsc_sz[TSC_ID]/(double)sam_id_sz),
            sam_nuc_sz,
            tsc_sz[TSC_NUC],
            (100*(double)tsc_sz[TSC_NUC]/(double)sam_nuc_sz),
            sam_pair_sz,
            tsc_sz[TSC_PAIR],
            (100*(double)tsc_sz[TSC_PAIR]/(double)sam_pair_sz),
            sam_qual_sz,
            tsc_sz[TSC_QUAL],
            (100*(double)tsc_sz[TSC_QUAL]/(double)sam_qual_sz),

            et[ET_TOT],
            (double)et[ET_TOT] / (double)1000000,
            (100*(double)et[ET_TOT]/(double)et[ET_TOT]),
            et[ET_AUX],
            (double)et[ET_AUX] / (double)1000000,
            (100*(double)et[ET_AUX]/(double)et[ET_TOT]),
            et[ET_ID],
            (double)et[ET_ID] / (double)1000000,
            (100*(double)et[ET_ID]/(double)et[ET_TOT]),
            et[ET_NUC],
            (double)et[ET_NUC] / (double)1000000,
            (100*(double)et[ET_NUC]/(double)et[ET_TOT]),
            et[ET_PAIR],
            (double)et[ET_PAIR] / (double)1000000,
            (100*(double)et[ET_PAIR]/(double)et[ET_TOT]),
            et[ET_QUAL],
            (double)et[ET_QUAL] / (double)1000000,
            (100*(double)et[ET_QUAL]/(double)et[ET_TOT]),
            et[ET_REM],
            (double)et[ET_REM] / (double)1000000,
            (100*(double)et[ET_REM] / (double)et[ET_TOT]),

            (sam_total_sz / MB) / ((double)et[ET_TOT] / (double)1000000),
            (sam_aux_sz / MB) / ((double)et[ET_AUX] / (double)1000000),
            (sam_id_sz / MB) / ((double)et[ET_ID] / (double)1000000),
            (sam_nuc_sz / MB) / ((double)et[ET_NUC] / (double)1000000),
            (sam_pair_sz / MB) / ((double)et[ET_PAIR] / (double)1000000),
            (sam_qual_sz / MB) / ((double)et[ET_QUAL] / (double)1000000));
}

void samenc_encode(samenc_t *samenc)
{
    size_t   sam_sz[14] = {0}; // SAM statistics
    size_t   tsc_sz[8]  = {0}; // Tsc statistics
    long     et[7]      = {0}; // Timing statistics
    long     fpos_prev  = 0;   // fp offset of the previous block
    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    tscfh_t *tscfh = tscfh_new();
    tscsh_t *tscsh = tscsh_new();
    tscbh_t *tscbh = tscbh_new();

    // Set up tsc file header, then seek past it for now
    tscfh->flags = 0x1; // LSB signals SAM
    tscfh->sblk_n = 3; // aux, nux, qual
    fseek(samenc->ofp, tscfh_size(tscfh), SEEK_SET);

    // Copy SAM header to tsc file
    sam_sz[SAM_HEAD] += tscsh->data_sz = (uint64_t)samenc->samparser->head->len;
    tscsh->data = (unsigned char *)samenc->samparser->head->s;
    tsc_sz[TSC_SH] = tscsh_write(tscsh, samenc->ofp);
    tscsh->data = NULL; // Need this before freeing tscsh

    // Set up block header
    tscbh->rec_n = samenc->blk_sz;

    samrec_t *samrec = &(samenc->samparser->curr);
    while (samparser_next(samenc->samparser)) {
        if (tscbh->rec_cnt >= tscbh->rec_n) {
            // Store the file pointer offset of this block in the previous
            // block header
            long fpos_curr = ftell(samenc->ofp);
            if (tscbh->blk_cnt > 0) {
                fseek(samenc->ofp, fpos_prev, SEEK_SET);
                fseek(samenc->ofp, (long)sizeof(tscbh->fpos), SEEK_CUR);
                fwrite_uint64(samenc->ofp, (uint64_t)fpos_curr);
                fseek(samenc->ofp, fpos_curr, SEEK_SET);
            }
            fpos_prev = fpos_curr;

            // Write block header
            tscbh->fpos = (uint64_t)ftell(samenc->ofp);
            tsc_sz[TSC_BH] += tscbh_write(tscbh, samenc->ofp);
            tscbh->blk_cnt++;
            tscbh->rec_cnt = 0;

            // Write sub-blocks
            gettimeofday(&tv0, NULL);
            tsc_sz[TSC_AUX]+=auxenc_write_block(samenc->auxenc, samenc->ofp);
            gettimeofday(&tv1, NULL);
            et[ET_AUX] += tvdiff(tv0, tv1);

            gettimeofday(&tv0, NULL);
            tsc_sz[TSC_ID]+=idenc_write_block(samenc->idenc, samenc->ofp);
            gettimeofday(&tv1, NULL);
            et[ET_ID] += tvdiff(tv0, tv1);

            gettimeofday(&tv0, NULL);
            tsc_sz[TSC_NUC]+=nucenc_write_block(samenc->nucenc, samenc->ofp);
            gettimeofday(&tv1, NULL);
            et[ET_NUC] += tvdiff(tv0, tv1);

            gettimeofday(&tv0, NULL);
            tsc_sz[TSC_PAIR]+=pairenc_write_block(samenc->pairenc, samenc->ofp);
            gettimeofday(&tv1, NULL);
            et[ET_PAIR] += tvdiff(tv0, tv1);

            gettimeofday(&tv0, NULL);
            tsc_sz[TSC_QUAL]+=qualenc_write_block(samenc->qualenc, samenc->ofp);
            gettimeofday(&tv1, NULL);
            et[ET_QUAL] += tvdiff(tv0, tv1);
        }

        // Add record to encoders
        gettimeofday(&tv0, NULL);
        auxenc_add_record(samenc->auxenc,
                          samrec->flag,
                          samrec->mapq,
                          samrec->opt);
        gettimeofday(&tv1, NULL);
        et[ET_AUX] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        idenc_add_record(samenc->idenc,
                         samrec->qname);
        gettimeofday(&tv1, NULL);
        et[ET_ID] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        nucenc_add_record(samenc->nucenc,
                          samrec->rname,
                          samrec->pos,
                          samrec->cigar,
                          samrec->seq);
        gettimeofday(&tv1, NULL);
        et[ET_NUC] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        pairenc_add_record(samenc->pairenc,
                           samrec->rnext,
                           samrec->pnext,
                           samrec->tlen);
        gettimeofday(&tv1, NULL);
        et[ET_QUAL] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        qualenc_add_record(samenc->qualenc,
                           samrec->qual);
        gettimeofday(&tv1, NULL);
        et[ET_QUAL] += tvdiff(tv0, tv1);

        // Accumulate input statistics
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
        sam_sz[SAM_CTRL]  += 11 /* \t's */ + 1 /* \n's */;

        tscbh->rec_cnt++;
        tscfh->rec_n++;
    }

    // Store the file pointer offset of this block in the previous
    // block header
    long fpos_curr = ftell(samenc->ofp);
    if (tscbh->blk_cnt > 0) {
        fseek(samenc->ofp, fpos_prev, SEEK_SET);
        fseek(samenc->ofp, (long)sizeof(tscbh->fpos), SEEK_CUR);
        fwrite_uint64(samenc->ofp, (uint64_t)fpos_curr);
        fseek(samenc->ofp, fpos_curr, SEEK_SET);
    }
    fpos_prev = fpos_curr;

    // Write -last- block header
    tscbh->fpos = (uint64_t)ftell(samenc->ofp);
    tscbh->fpos_nxt = 0; // Last block header has a zero here
    tsc_sz[TSC_BH] += tscbh_write(tscbh, samenc->ofp);
    tscbh->blk_cnt++;
    tscbh->rec_cnt = 0;

    // Write -last- sub-blocks
    gettimeofday(&tv0, NULL);
    tsc_sz[TSC_AUX]+=auxenc_write_block(samenc->auxenc, samenc->ofp);
    gettimeofday(&tv1, NULL);
    et[ET_AUX] += tvdiff(tv0, tv1);

    gettimeofday(&tv0, NULL);
    tsc_sz[TSC_ID]+=idenc_write_block(samenc->idenc, samenc->ofp);
    gettimeofday(&tv1, NULL);
    et[ET_ID] += tvdiff(tv0, tv1);

    gettimeofday(&tv0, NULL);
    tsc_sz[TSC_NUC]+=nucenc_write_block(samenc->nucenc, samenc->ofp);
    gettimeofday(&tv1, NULL);
    et[ET_NUC] += tvdiff(tv0, tv1);

    gettimeofday(&tv0, NULL);
    tsc_sz[TSC_PAIR]+=pairenc_write_block(samenc->pairenc, samenc->ofp);
    gettimeofday(&tv1, NULL);
    et[ET_PAIR] += tvdiff(tv0, tv1);

    gettimeofday(&tv0, NULL);
    tsc_sz[TSC_QUAL]+=qualenc_write_block(samenc->qualenc, samenc->ofp);
    gettimeofday(&tv1, NULL);
    et[ET_QUAL] += tvdiff(tv0, tv1);

    // Write file header
    tscfh->blk_n = tscbh->blk_cnt;
    rewind(samenc->ofp);
    tsc_sz[TSC_FH] = tscfh_write(tscfh, samenc->ofp);
    fseek(samenc->ofp, (long)0, SEEK_END);

    // Print nuccodec summary
    tsc_vlog("Nuccodec: %zu invalid record(s)\n", samenc->nucenc->m_tot_cnt);
    tsc_vlog("Nuccodec: %zu added I-Record(s)\n", samenc->nucenc->i_tot_cnt);

    // Print summary
    gettimeofday(&tt1, NULL);
    et[ET_TOT] = tvdiff(tt0, tt1);
    et[ET_REM] = et[ET_TOT] - et[ET_AUX] - et[ET_ID]
               - et[ET_NUC] - et[ET_PAIR] - et[ET_QUAL];
    tsc_vlog("Compressed %zu record(s)\n", tscfh->rec_n);
    tsc_vlog("Wrote %zu block(s)\n", tscfh->blk_n);
    tsc_vlog("Took %ld us ~= %.2f s\n", et[ET_TOT], (double)et[ET_TOT]/1000000);

    // If selected, print detailed statistics
    if (tsc_stats) samenc_print_stats(sam_sz, tsc_sz, tscfh, et);

    tscfh_free(tscfh);
    tscsh_free(tscsh);
    tscbh_free(tscbh);
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
    samdec->iddec = iddec_new();
    samdec->nucdec = nucdec_new();
    samdec->pairdec = pairdec_new();
    samdec->qualdec = qualdec_new();
    samdec_init(samdec, ifp, ofp);
    return samdec;
}

void samdec_free(samdec_t *samdec)
{
    if (samdec != NULL) {
        auxdec_free(samdec->auxdec);
        iddec_free(samdec->iddec);
        nucdec_free(samdec->nucdec);
        pairdec_free(samdec->pairdec);
        qualdec_free(samdec->qualdec);
        free(samdec);
        samdec = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void samdec_print_stats(const size_t  *sam_sz,
                               const size_t  *tsc_sz,
                               const tscfh_t *tscfh,
                               const long    *et)
{
    samenc_print_stats(sam_sz, tsc_sz, tscfh, et);
}

void samdec_decode(samdec_t *samdec)
{
    size_t   sam_sz[14] = {0}; // SAM statistics
    size_t   tsc_sz[8]  = {0}; // Tsc statistics
    long     et[7]      = {0}; // Timing statistics
    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    tscfh_t *tscfh = tscfh_new();
    tscsh_t *tscsh = tscsh_new();
    tscbh_t *tscbh = tscbh_new();

    // Read and check file header
    tsc_sz[TSC_FH] = tscfh_read(tscfh, samdec->ifp);

    // Read SAM header from tsc file and write it to SAM file
    tsc_sz[TSC_SH] = tscsh_read(tscsh, samdec->ifp);
    sam_sz[SAM_HEAD] += fwrite_buf(samdec->ofp, tscsh->data, tscsh->data_sz);

    size_t b = 0;
    for (b = 0; b < tscfh->blk_n; b++) {
        tsc_sz[TSC_BH] += tscbh_read(tscbh, samdec->ifp);

        // Allocate memory to prepare decoding of the sub-blocks
        str_t   **qname=(str_t **)tsc_malloc(sizeof(str_t *)*tscbh->rec_cnt);
        uint16_t *flag =(uint16_t *)tsc_malloc(sizeof(uint16_t)*tscbh->rec_cnt);
        str_t   **rname=(str_t **)tsc_malloc(sizeof(str_t *)*tscbh->rec_cnt);
        uint32_t *pos  =(uint32_t *)tsc_malloc(sizeof(uint32_t)*tscbh->rec_cnt);
        uint8_t *mapq  =(uint8_t *)tsc_malloc(sizeof(uint8_t)*tscbh->rec_cnt);
        str_t   **cigar=(str_t **)tsc_malloc(sizeof(str_t *)*tscbh->rec_cnt);
        str_t   **rnext=(str_t **)tsc_malloc(sizeof(str_t *)*tscbh->rec_cnt);
        uint32_t *pnext=(uint32_t *)tsc_malloc(sizeof(uint32_t)*tscbh->rec_cnt);
        int64_t *tlen  =(int64_t *)tsc_malloc(sizeof(int64_t)*tscbh->rec_cnt);
        str_t   **seq  =(str_t **)tsc_malloc(sizeof(str_t *)*tscbh->rec_cnt);
        str_t   **qual =(str_t **)tsc_malloc(sizeof(str_t *)*tscbh->rec_cnt);
        str_t   **opt  =(str_t **)tsc_malloc(sizeof(str_t *)*tscbh->rec_cnt);

        size_t r = 0;
        for (r = 0; r < tscbh->rec_cnt; r++) {
            qname[r] = str_new();
            rname[r] = str_new();
            cigar[r] = str_new();
            rnext[r] = str_new();
            seq[r]   = str_new();
            qual[r]  = str_new();
            opt[r]   = str_new();
        }

        // Decode sub-blocks
        gettimeofday(&tv0, NULL);
        tsc_sz[TSC_AUX] += auxdec_decode_block(samdec->auxdec,
                                               samdec->ifp,
                                               flag,
                                               mapq,
                                               opt);
        gettimeofday(&tv1, NULL);
        et[ET_AUX] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        tsc_sz[TSC_ID] += iddec_decode_block(samdec->iddec,
                                             samdec->ifp,
                                             qname);
        gettimeofday(&tv1, NULL);
        et[ET_ID] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        tsc_sz[TSC_NUC] += nucdec_decode_block(samdec->nucdec,
                                               samdec->ifp,
                                               rname,
                                               pos,
                                               cigar,
                                               seq);
        gettimeofday(&tv1, NULL);
        et[ET_NUC] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        tsc_sz[TSC_PAIR] += pairdec_decode_block(samdec->pairdec,
                                                 samdec->ifp,
                                                 rnext,
                                                 pnext,
                                                 tlen);
        gettimeofday(&tv1, NULL);
        et[ET_PAIR] += tvdiff(tv0, tv1);

        gettimeofday(&tv0, NULL);
        tsc_sz[TSC_QUAL] += qualdec_decode_block(samdec->qualdec,
                                                 samdec->ifp,
                                                 qual);
        gettimeofday(&tv1, NULL);
        et[ET_QUAL] += tvdiff(tv0, tv1);

        // These are dummies for testing
        //int i = 0;
        //for (i = 0; i < tscbh->rec_cnt; i++) {
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
        for (r = 0; r < tscbh->rec_cnt; r++) {
            // Convert int-fields to C-strings (101 bytes should be enough)
            char flag_cstr[101];
            char pos_cstr[101];
            char mapq_cstr[101];
            char pnext_cstr[101];
            char tlen_cstr[101];

            snprintf(flag_cstr, sizeof(flag_cstr), "%"PRIu16, flag[r]);
            snprintf(pos_cstr, sizeof(pos_cstr), "%"PRIu32, pos[r]);
            snprintf(mapq_cstr, sizeof(mapq_cstr), "%"PRIu8, mapq[r]);
            snprintf(pnext_cstr, sizeof(pnext_cstr), "%"PRIu32, pnext[r]);
            snprintf(tlen_cstr, sizeof(tlen_cstr), "%"PRId64, tlen[r]);

            char *sam_fields[12];
            sam_fields[SAM_QNAME] = qname[r]->s;
            sam_fields[SAM_FLAG]  = flag_cstr;
            sam_fields[SAM_RNAME] = rname[r]->s;
            sam_fields[SAM_POS]   = pos_cstr;
            sam_fields[SAM_MAPQ]  = mapq_cstr;
            sam_fields[SAM_CIGAR] = cigar[r]->s;
            sam_fields[SAM_RNEXT] = rnext[r]->s;
            sam_fields[SAM_PNEXT] = pnext_cstr;
            sam_fields[SAM_TLEN]  = tlen_cstr;
            sam_fields[SAM_SEQ]   = seq[r]->s;
            sam_fields[SAM_QUAL]  = qual[r]->s;
            sam_fields[SAM_OPT]   = opt[r]->s;

            // Write data to file
            size_t f = 0;
            for (f = 0; f < 12; f++) {
                sam_sz[f] += fwrite_buf(samdec->ofp,
                                        (unsigned char *)sam_fields[f],
                                        strlen(sam_fields[f]));
                if (f != 11 && strlen(sam_fields[f + 1]))
                    sam_sz[SAM_CTRL] += fwrite_byte(samdec->ofp, '\t');
            }
            sam_sz[SAM_CTRL] += fwrite_byte(samdec->ofp, '\n');

            // Free the memory used for the current line
            str_free(qname[r]);
            str_free(rname[r]);
            str_free(cigar[r]);
            str_free(rnext[r]);
            str_free(seq[r]);
            str_free(qual[r]);
            str_free(opt[r]);
        }

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
    }

    // Print summary
    gettimeofday(&tt1, NULL);
    et[ET_TOT] = tvdiff(tt0, tt1);
    et[ET_REM] = et[ET_TOT] - et[ET_AUX] - et[ET_ID]
               - et[ET_NUC] - et[ET_PAIR] - et[ET_QUAL];
    tsc_vlog("Decompressed %zu record(s)\n", tscfh->rec_n);
    tsc_vlog("Read %zu block(s)\n", tscfh->blk_n);
    tsc_vlog("Took %ld us ~= %.2f s\n", et[ET_TOT], (double)et[ET_TOT]/1000000);

    // If selected, print detailed statistics
    if (tsc_stats) samdec_print_stats(sam_sz, tsc_sz, tscfh, et);

    tscfh_free(tscfh);
    tscsh_free(tscsh);
    tscbh_free(tscbh);
}

void samdec_info(samdec_t *samdec)
{
    tscfh_t *tscfh = tscfh_new();
    tscsh_t *tscsh = tscsh_new();
    tscbh_t *tscbh = tscbh_new();

    // Read and check file header
    tscfh_read(tscfh, samdec->ifp);

    // Skip SAM header
    tscsh_read(tscsh, samdec->ifp);

    // Read and print block headers
    tsc_log("\n"
            "\t        fpos      fpos_nxt       blk_cnt       rec_cnt"
            "         rec_n       chr_cnt       pos_min       pos_max\n");

    while (1) {
        tscbh_read(tscbh, samdec->ifp);

        printf("\t");
        printf("%12"PRIu64"  ", tscbh->fpos);
        printf("%12"PRIu64"  ", tscbh->fpos_nxt);
        printf("%12"PRIu64"  ", tscbh->blk_cnt);
        printf("%12"PRIu64"  ", tscbh->rec_cnt);
        printf("%12"PRIu64"  ", tscbh->rec_n);
        printf("%12"PRIu64"  ", tscbh->chr_cnt);
        printf("%12"PRIu64"  ", tscbh->pos_min);
        printf("%12"PRIu64"  ", tscbh->pos_max);
        printf("\n");

        if (tscbh->fpos_nxt)
            fseek(samdec->ifp, (long)tscbh->fpos_nxt, SEEK_SET);
        else
            break; // Last block has zeros here
    }
    printf("\n");

    tscfh_free(tscfh);
    tscsh_free(tscsh);
    tscbh_free(tscbh);
}

