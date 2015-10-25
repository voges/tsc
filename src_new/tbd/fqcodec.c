#include "filecodec.h"
#include "./frw/frw.h"
#include "gomplib.h"
#include "version.h"
#include <inttypes.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Encoder
////////////////////////////////////////////////////////////////////////////////
static void fqenc_init(fqenc_t *fqenc, FILE *ifp, FILE *ofp)
{
    fqenc->ifp = ifp;
    fqenc->ofp = ofp;
}

fqenc_t * fqenc_new(FILE *ifp, FILE *ofp)
{
    fqenc_t *fqenc = (fqenc_t *)gomp_malloc(sizeof(fqenc_t));
    fqenc->fqparser = fqparser_new(ifp);
    fqenc->rheadenc = rheadenc_new();
    fqenc->seqenc = seqenc_new();
    fqenc->qualenc = qualenc_new();
    fqenc_init(fqenc, ifp, ofp);
    return fqenc;
}

void fqenc_free(fqenc_t *fqenc)
{
    if (fqenc) {
        fqparser_free(fqenc->fqparser);
        rheadenc_free(fqenc->rheadenc);
        seqenc_free(fqenc->seqenc);
        qualenc_free(fqenc->qualenc);
        free(fqenc);
        fqenc = NULL;
    } else {
        gomp_error("Tried to free null pointer\n");
    }
}

static void fqenc_print_stats(const size_t* sam_sz, const size_t* gomp_sz,
                                const size_t blk_cnt, const size_t line_cnt)
{
    gomp_log("\n"
            "\tCompression Statistics:\n"
            "\t-----------------------\n"
            "\tNumber of FASTQ records processed: %12zu\n"
            "\tNumber of records per block      : %12zu\n"
            "\tNumber of blocks written         : %12zu\n"
            "\tBytes read                       : %12zu (%6.2f%%)\n"
            "\t  RNAME                          : %12zu (%6.2f%%)\n"
            "\t  SEQ                            : %12zu (%6.2f%%)\n"
            "\t  DESC                           : %12zu (%6.2f%%)\n"
            "\t  QUAL                           : %12zu (%6.2f%%)\n"
            "\tBytes written                    : %12zu (%6.2f%%)\n"
            "\t  File format                    : %12zu (%6.2f%%)\n"
            "\t  RNAME                          : %12zu (%6.2f%%)\n"
            "\t  SEQ                            : %12zu (%6.2f%%)\n"
            "\t  DESC                           : %12zu (%6.2f%%)\n"
            "\t  QUAL                           : %12zu (%6.2f%%)\n"
            "\tCompression ratios                         read /      written\n"
            "\t  RNAME                          : %12zu / %12zu (%6.2f%%)\n"
            "\t  SEQ                            : %12zu / %12zu (%6.2f%%)\n"
            "\t  DESC                           : %12zu / %12zu (%6.2f%%)\n"
            "\t  QUAL                           : %12zu / %12zu (%6.2f%%)\n"
            "\n",
            blk_cnt,
            line_cnt,
            gomp_sz[gomp_TOTAL],
            (100 * (double)gomp_sz[gomp_TOTAL]
             / (double)gomp_sz[gomp_TOTAL]),
            gomp_sz[gomp_FF],
            (100 * (double)gomp_sz[gomp_FF]
             / (double)gomp_sz[gomp_TOTAL]),
            gomp_sz[gomp_SH],
            (100 * (double)gomp_sz[gomp_SH]
             / (double)gomp_sz[gomp_TOTAL]),
            gomp_sz[gomp_AUX],
            (100 * (double)gomp_sz[gomp_AUX]
             / (double)gomp_sz[gomp_TOTAL]),
            gomp_sz[gomp_NUC],
            (100 * (double)gomp_sz[gomp_NUC]
             / (double)gomp_sz[gomp_TOTAL]),
            gomp_sz[gomp_QUAL],
            (100 * (double)gomp_sz[gomp_QUAL]
             / (double)gomp_sz[gomp_TOTAL]),
            sam_sz[SAM_SEQ], gomp_sz[gomp_NUC],
            (100 * (double)gomp_sz[gomp_NUC]
             / (double)sam_sz[SAM_SEQ]),
            sam_sz[SAM_QUAL], gomp_sz[gomp_QUAL],
            (100 * (double)gomp_sz[gomp_QUAL]
             / (double)sam_sz[SAM_QUAL]));
}

static void fqenc_print_time(long elapsed_total, long elapsed_pred,
                               long elapsed_entr)
{
    gomp_log("\n"
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

void fqenc_encode(fqenc_t* fqenc)
{
    size_t fq_sz[4] = {0}; // SAM statistics                  
    size_t gomp_sz[4]  = {0}; // gomp statistics                  
    size_t line_cnt = 0;     // line counter                    
    long elapsed_total = 0;  // total time used for encoding    
    long elapsed_pred  = 0;  // time used for predictive coding 
    long elapsed_entr  = 0;  // time used for entropy coding    

    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    uint64_t blk_cnt  = 0; // for the block header 
    uint64_t blkl_cnt = 0; // for the block header 

    uint64_t* lut = (uint64_t*)malloc(0);
    size_t lut_sz = 0; // for the LUT header 
    size_t lut_itr = 0;

    gompfmt_write_gomphead(gompfmt_header_type, fqenc->ofp, gomp_sz);

    fqrec_t* fqrec = &(fqenc->fqparser->curr);
    while (fqparser_next(fqenc->samparser)) {
        if (blkl_cnt >= FILECODEC_BLK_LC) {
            // Retain information for LUT entry 
            lut_sz += 4 * sizeof(uint64_t);
            lut = (uint64_t *)gomp_realloc(lut, lut_sz);
            lut[lut_itr++] = blk_cnt;
            lut[lut_itr++] = (uint64_t)ftell(fqenc->ofp);

            // Block header 
            gompfmt_write_blkhead(fqenc->ofp, gomp_sz, blk_cnt, blkl_cnt);
            
            gomp_sz[gomp_FF] += fwrite_uint64(fqenc->ofp, blk_cnt);
            gomp_sz[gomp_FF] += fwrite_uint64(fqenc->ofp, blkl_cnt);

            // Sub-blocks 
            gettimeofday(&tv0, NULL);
            gomp_sz[gomp_AUX]
                += auxenc_write_block(fqenc->auxenc, fqenc->ofp);
            gomp_sz[gomp_NUC]
                += nucenc_write_block(fqenc->nucenc, fqenc->ofp);
            gomp_sz[gomp_QUAL]
                += qualenc_write_block(fqenc->qualenc, fqenc->ofp);
            gettimeofday(&tv1, NULL);
            elapsed_entr += tvdiff(tv0, tv1);

            gomp_vlog("Wrote block %zu: %zu line(s)\n", blk_cnt, blkl_cnt);

            blk_cnt++;
            blkl_cnt = 0;
        }

        // Add records to encoders 
        gettimeofday(&tv0, NULL);
        rnameenc_add_record(fqenc->rnameenc, fqrec->rname);
        seqenc_add_record(fqenc->seqenc, fqrec->seq);
        qualenc_add_record(fqenc->qualenc, fqrec->qual);
        gettimeofday(&tv1, NULL);
        elapsed_pred += tvdiff(tv0, tv1);

        // Accumulate input statistics 
        fq_sz[FQ_RNAME] += strlen(fqrec->rname);
        fq_sz[SAM_SEQ] += strlen(fqrec->seq);
        fq_sz[SAM_QUAL] += strlen(fqqrec->qual);

        blkl_cnt++;
        line_cnt++;
    }

    // Retain information for LUT entry 
    lut_sz += 4 * sizeof(uint64_t);
    lut = (uint64_t*)gomp_realloc(lut, lut_sz);
    lut[lut_itr++] = blk_cnt;
    lut[lut_itr++] = (uint64_t)ftell(fqenc->ofp);

    // Block header (last) 
    gomp_sz[gomp_FF] += fwrite_uint64(fqenc->ofp, blk_cnt);
    gomp_sz[gomp_FF] += fwrite_uint64(fqenc->ofp, blkl_cnt);

    // Sub-blocks (last) 
    gettimeofday(&tv0, NULL);
    gomp_sz[gomp_AUX]
        += auxenc_write_block(fqenc->auxenc, fqenc->ofp);
    gomp_sz[gomp_NUC]
        += nucenc_write_block(fqenc->nucenc, fqenc->ofp);
    gomp_sz[gomp_QUAL]
        += qualenc_write_block(fqenc->qualenc, fqenc->ofp);
    gettimeofday(&tv1, NULL);
    elapsed_entr += tvdiff(tv0, tv1);

    gomp_vlog("Wrote last block %zu: %zu line(s)\n", blk_cnt, blkl_cnt);

    blk_cnt++;

    // Complete file header with LUT information. 
    lut_pos = (uint64_t)ftell(fqenc->ofp);
    fseek(fqenc->ofp, sizeof(magic)+sizeof(version)+sizeof(blk_lc), SEEK_SET);
    gomp_sz[gomp_FF] += fwrite_uint64(fqenc->ofp, blk_cnt);
    gomp_sz[gomp_FF] += fwrite_uint64(fqenc->ofp, lut_pos);
    fseek(fqenc->ofp, 0, SEEK_END);

    // LUT header 
    unsigned char lut_magic[8] = "lut----"; lut_magic[7] = '\0';
    //uint64_t      lut_sz       = (uint64_t)lut_itr * sizeof(uint64_t);

    gomp_sz[gomp_FF] += fwrite_buf(fqenc->ofp, lut_magic, sizeof(lut_magic));
    gomp_sz[gomp_FF] += fwrite_uint64(fqenc->ofp, lut_sz);

    gomp_vlog("Wrote LUT header\n");

    // LUT entries 
    gomp_sz[gomp_FF] += fwrite_buf(fqenc->ofp, (unsigned char*)lut, lut_sz);
    if (!lut) {
        gomp_error("Tried to free NULL pointer!\n");
    } else {
        free(lut);
        lut = NULL;
    }

    gomp_vlog("Wrote LUT entries\n");

    // Print summary 
    gettimeofday(&tt1, NULL);
    elapsed_total = tvdiff(tt0, tt1);
    gomp_sz[gomp_TOTAL] = gomp_sz[gomp_FF]
                                 + gomp_sz[gomp_SH]
                                 + gomp_sz[gomp_NUC]
                                 + gomp_sz[gomp_QUAL]
                                 + gomp_sz[gomp_AUX];
    gomp_vlog("Wrote %zu bytes ~= %.2f GiB (%zu line(s) in %zu block(s))\n",
             gomp_sz[gomp_TOTAL], ((double)gomp_sz[gomp_TOTAL] / GB), line_cnt,
             blk_cnt);
    gomp_vlog("Took %ld us ~= %.4f s\n", elapsed_total,
             (double)elapsed_total / 1000000);

    // If selected by the user, print detailed statistics and/or timing info. 
    if (gomp_stats) fqenc_print_stats(sam_sz, gomp_sz, blk_cnt, line_cnt);
    if (gomp_time) fqenc_print_time(elapsed_total, elapsed_pred, elapsed_entr);
}

//*****************************************************************************
 * Decoder                                                                    *
 *****************************************************************************
static void fqdec_init(fqdec_t* fqdec, FILE* ifp, FILE* ofp)
{
    fqdec->ifp = ifp;
    fqdec->ofp = ofp;
}

fqdec_t* fqdec_new(FILE* ifp, FILE* ofp)
{
    fqdec_t* fqdec = (fqdec_t *)gomp_malloc(sizeof(fqdec_t));
    fqdec->nucdec = nucdec_new();
    fqdec->qualdec = qualdec_new();
    fqdec->auxdec = auxdec_new();
    fqdec_init(fqdec, ifp, ofp);
    return fqdec;
}

void fqdec_free(fqdec_t* fqdec)
{
    if (fqdec != NULL) {
        nucdec_free(fqdec->nucdec);
        qualdec_free(fqdec->qualdec);
        auxdec_free(fqdec->auxdec);
        free(fqdec);
        fqdec = NULL;
    } else { // fqdec == NULL 
        gomp_error("Tried to free NULL pointer.\n");
    }
}

static void fqdec_print_stats(fqdec_t* fqdec, const size_t blk_cnt,
                                const size_t line_cnt, const size_t sam_sz)
{
    gomp_log("\n"
            "\tDecompression Statistics:\n"
            "\t-------------------------\n"
            "\tNumber of blocks decoded      : %12zu\n"
            "\tSAM records (lines) processed : %12zu\n"
            "\tTotal bytes written           : %12zu\n"
            "\n",
            blk_cnt,
            line_cnt,
            sam_sz);
}

static void fqdec_print_time(long elapsed_total, long elapsed_dec,
                               long elapsed_wrt)
{
    gomp_log("\n"
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

void fqdec_decode(fqdec_t* fqdec)
{
    size_t sam_sz      = 0; // total no. of bytes written   
    size_t line_cnt    = 0; // line counter                 
    long elapsed_total = 0; // total time used for encoding 
    long elapsed_dec   = 0; // time used for decoding       
    long elapsed_wrt   = 0; // time used for writing        

    struct timeval tt0, tt1, tv0, tv1;
    gettimeofday(&tt0, NULL);

    uint64_t blk_cnt;  // for the block header 
    uint64_t blkl_cnt; // for the block header 

    // File header 
    unsigned char magic[6];
    unsigned char version[6];
    uint64_t      blk_lc;
    uint64_t      blk_n;
    uint64_t      lut_pos;

    if (fread_buf(fqdec->ifp, magic, sizeof(magic)) != sizeof(magic))
        gomp_error("Failed to read magic!\n");
    if (fread_buf(fqdec->ifp, version, sizeof(version)) != sizeof(version))
        gomp_error("Failed to read version!\n");
    if (fread_uint64(fqdec->ifp, &blk_lc) != sizeof(blk_lc))
        gomp_error("Failed to read no. of lines per block!\n");
    if (fread_uint64(fqdec->ifp, &blk_n) != sizeof(blk_n))
        gomp_error("Failed to read number of blocks!\n");
    if (fread_uint64(fqdec->ifp, &lut_pos) != sizeof(lut_pos))
        gomp_error("Failed to read LUT position!\n");

    if (strncmp((const char*)version, (const char*)gomp_version->s,
                sizeof(version)))
        gomp_error("File was compressed with another version: %s\n", version);

    magic[3] = '\0';
    gomp_vlog("Format: %s %s\n", magic, version);
    gomp_vlog("Block size: %zu\n", (size_t)blk_lc);

    // SAM header 
    uint64_t       header_sz;
    unsigned char* header;

    if (fread_uint64(fqdec->ifp, &header_sz) != sizeof(header_sz))
        gomp_error("Failed to read SAM header size!\n");
    header = (unsigned char*)gomp_malloc((size_t)header_sz);
    if (fread_buf(fqdec->ifp, header, header_sz) != header_sz)
        gomp_error("Failed to read SAM header!\n");
    sam_sz += fwrite_buf(fqdec->ofp, header, header_sz);
    free(header);

    gomp_vlog("Read SAM header\n");

    size_t i = 0;
    for (i = 0; i < blk_n; i++) {
        // Block header 
        if (fread_uint64(fqdec->ifp, &blk_cnt) != sizeof(blk_cnt))
            gomp_error("Failed to read block count!\n");
        if (fread_uint64(fqdec->ifp, &blkl_cnt) != sizeof(blkl_cnt))
            gomp_error("Failed to read no. of lines in block %zu!\n",
                      (size_t)blk_cnt);

        gomp_vlog("Decoding block %zu: %zu lines\n", (size_t)blk_cnt,
                 (size_t)blkl_cnt);

        // Allocate memory to prepare decoding of the sub-blocks. 
        str_t**  qname = (str_t**)gomp_malloc(sizeof(str_t*) * blkl_cnt);
        int64_t* flag  = (int64_t*)gomp_malloc(sizeof(int64_t) * blkl_cnt);
        str_t**  rname = (str_t**)gomp_malloc(sizeof(str_t*) * blkl_cnt);
        int64_t* pos   = (int64_t*)gomp_malloc(sizeof(int64_t) * blkl_cnt);
        int64_t* mapq  = (int64_t*)gomp_malloc(sizeof(int64_t) * blkl_cnt);
        str_t**  cigar = (str_t**)gomp_malloc(sizeof(str_t*) * blkl_cnt);
        str_t**  rnext = (str_t**)gomp_malloc(sizeof(str_t*) * blkl_cnt);
        int64_t* pnext = (int64_t*)gomp_malloc(sizeof(int64_t) * blkl_cnt);
        int64_t* tlen  = (int64_t*)gomp_malloc(sizeof(int64_t) * blkl_cnt);
        str_t**  seq   = (str_t**)gomp_malloc(sizeof(str_t*) * blkl_cnt);
        str_t**  qual  = (str_t**)gomp_malloc(sizeof(str_t*) * blkl_cnt);
        str_t**  opt   = (str_t**)gomp_malloc(sizeof(str_t*) * blkl_cnt);

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

        // Decode sub-blocks 
        gettimeofday(&tv0, NULL);

        auxdec_decode_block(fqdec->auxdec, fqdec->ifp, qname, flag, rname,
                            mapq, rnext, pnext, tlen, opt);
        nucdec_decode_block(fqdec->nucdec, fqdec->ifp, pos, cigar, seq);
        qualdec_decode_block(fqdec->qualdec, fqdec->ifp, qual);

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

        // Write decoded sub-blocks in correct order to outfile. 
        gettimeofday(&tv0, NULL);

        for (i = 0; i < blkl_cnt; i++) {
            // Convert int-fields to C-strings 
            char flag_cstr[101];  // this should be enough space 
            char pos_cstr[101];   // this should be enough space 
            char mapq_cstr[101];  // this should be enough space 
            char pnext_cstr[101]; // this should be enough space 
            char tlen_cstr[101];  // this should be enough space 
            snprintf(flag_cstr, sizeof(flag_cstr), "%"PRId64, flag[i]);
            snprintf(pos_cstr, sizeof(pos_cstr), "%"PRId64, pos[i]);
            snprintf(mapq_cstr, sizeof(mapq_cstr), "%"PRId64, mapq[i]);
            snprintf(pnext_cstr, sizeof(pnext_cstr), "%"PRId64, pnext[i]);
            snprintf(tlen_cstr, sizeof(tlen_cstr), "%"PRId64, tlen[i]);

            // Set pointers to C-strings 
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

            // Write data to file 
            size_t f = 0;
            for (f = 0; f < 12; f++) {
                sam_sz += fwrite_buf(fqdec->ofp,
                                     (unsigned char*)sam_fields[f],
                                     strlen(sam_fields[f]));
                if (f != 11 && strlen(sam_fields[f + 1]))
                    sam_sz += fwrite_byte(fqdec->ofp, '\t');
            }
            sam_sz += fwrite_byte(fqdec->ofp, '\n');

            // Free the memory used for the current line. 
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

        // Free memory allocated for this block. 
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

    // Print summary 
    gomp_vlog("Decoded %zu line(s) in %zu block(s) and wrote %zu bytes ~= "
             "%.2f GiB\n",
             line_cnt, (size_t)blk_cnt, sam_sz, ((double)sam_sz / GB));
    gomp_vlog("Took %ld us ~= %.4f s\n", elapsed_total,
             (double)elapsed_total / 1000000);

    // If selected by the user, print detailed statistics. 
    if (gomp_stats)
        fqdec_print_stats(fqdec, (size_t)blk_cnt, line_cnt, sam_sz);
    if (gomp_time)
        fqdec_print_time(elapsed_total, elapsed_dec, elapsed_wrt);
}

void fqdec_info(fqdec_t* fqdec)
{
    // File header 
    unsigned char magic[6];
    unsigned char version[6];
    uint64_t      blk_lc;
    uint64_t      blk_n;
    uint64_t      lut_pos;

    if (fread_buf(fqdec->ifp, magic, sizeof(magic)) != sizeof(magic))
        gomp_error("Failed to read magic!\n");
    if (fread_buf(fqdec->ifp, version, sizeof(version)) != sizeof(version))
        gomp_error("Failed to read version!\n");
    if (fread_uint64(fqdec->ifp, &blk_lc) != sizeof(blk_lc))
        gomp_error("Failed to read no. of lines per block!\n");
    if (fread_uint64(fqdec->ifp, &blk_n) != sizeof(blk_n))
        gomp_error("Failed to read number of blocks!\n");
    if (fread_uint64(fqdec->ifp, &lut_pos) != sizeof(lut_pos))
        gomp_error("Failed to read LUT position!\n");

    if (strncmp((const char*)version, (const char*)gomp_version->s,
                sizeof(version)))
        gomp_error("File was compressed with another version: %s\n", version);

    magic[3] = '\0';
    gomp_log("Format: %s %s\n", magic, version);
    gomp_log("Block size: %zu\n", (size_t)blk_lc);
    gomp_log("Number of blocks: %zu\n", (size_t)blk_n);

    // Seek to LUT position 
    fseek(fqdec->ifp, (long)lut_pos, SEEK_SET);

    // LUT header 
    unsigned char lut_magic[8];
    uint64_t      lut_sz;

    if (fread_buf(fqdec->ifp, lut_magic, sizeof(lut_magic))
        != sizeof(lut_magic))
        gomp_error("Failed to read LUT magic!\n");
    if (fread_uint64(fqdec->ifp, &lut_sz) != sizeof(lut_sz))
        gomp_error("Failed to read LUT size!\n");

    if (strncmp((const char*)lut_magic, "lut----", 7))
        gomp_error("Wrong LUT magic!\n");

    // Block LUT entries 
    uint64_t* lut = (uint64_t*)gomp_malloc(lut_sz);

    if (fread_buf(fqdec->ifp, (unsigned char*)lut, lut_sz) != lut_sz)
        gomp_error("Failed to read LUT entries!\n");

    gomp_log("\n"
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








void filedec_info(fqdec_t* filedec)
{
    // File header 
    gompfmt_read_fhead();

    // Skip SAM header 
    uint64_t sh_sz = 0;
    if (fread_uint64(filedec->ifp, &sh_sz) != sizeof(sh_sz))
        tsc_error("Failed to read SAM header size!\n");
    fseek(filedec->ifp, (long)sh_sz, SEEK_CUR);

    // Block header variables 
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
        fread_uint64(filedec->ifp, &bh_fpos);
        fread_uint64(filedec->ifp, &bh_fpos_nxt);
        fread_uint64(filedec->ifp, &bh_blk_cnt);
        fread_uint64(filedec->ifp, &bh_line_cnt);
        fread_uint64(filedec->ifp, &bh_chr_cnt);
        fread_uint64(filedec->ifp, &bh_pos_min);
        fread_uint64(filedec->ifp, &bh_pos_max);

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
            fseek(filedec->ifp, (long)bh_fpos_nxt, SEEK_SET);
        else
            break;
    }
    printf("\n");
}

