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
#include <string.h>

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void fileenc_init(fileenc_t* fileenc, FILE* ifp, FILE* ofp, const size_t block_sz)
{
    fileenc->ifp = ifp;
    fileenc->ofp = ofp;
    fileenc->block_sz = block_sz;
}

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp, const size_t block_sz)
{
    fileenc_t* fileenc = (fileenc_t *)tsc_malloc_or_die(sizeof(fileenc_t));
    fileenc->samparser = samparser_new(ifp);
    fileenc->seqenc = seqenc_new(block_sz);
    fileenc->qualenc = qualenc_new(block_sz);
    fileenc->auxenc = auxenc_new(block_sz);
    fileenc->stats = str_new();
    fileenc_init(fileenc, ifp, ofp, block_sz);
    return fileenc;
}

void fileenc_free(fileenc_t* fileenc)
{
    if (fileenc != NULL) {
        samparser_free(fileenc->samparser);
        seqenc_free(fileenc->seqenc);
        qualenc_free(fileenc->qualenc);
        auxenc_free(fileenc->auxenc);
        str_free(fileenc->stats);
        free((void*)fileenc);
        fileenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

str_t* fileenc_encode(fileenc_t* fileenc)
{
    size_t total_bc = 0; /* total byte count */
    size_t total_seq_bc = 0;
    size_t total_qual_bc = 0;
    size_t total_aux_bc = 0;

    /*
     * Write tsc file header:
     * - 4 bytes: 'TSC-' + 0x00
     * - 4 bytes: version + 0x00
     * - 4 bytes: block size used for encoding
     */
    tsc_log("Format: %s %s\n", tsc_prog_name->s, tsc_version->s);
    total_bc += fwrite_cstr(fileenc->ofp, tsc_prog_name->s);
    total_bc += fwrite_byte(fileenc->ofp, 0x00);
    total_bc += fwrite_cstr(fileenc->ofp, tsc_version->s);
    total_bc += fwrite_byte(fileenc->ofp, 0x00);
    tsc_log("Block size: %zu\n", fileenc->block_sz);
    total_bc += fwrite_uint32(fileenc->ofp, fileenc->block_sz);

    /*
     * Write SAM header:
     * - 4 bytes: number of bytes in header
     * - n bytes: SAM header
     */
    tsc_log("Writing SAM header\n");
    total_bc += fwrite_uint32(fileenc->ofp, fileenc->samparser->head->n);
    total_bc += fwrite_cstr(fileenc->ofp, fileenc->samparser->head->s);

    /* Prepare encoding */
    size_t block_cnt = 0; /* block count */
    size_t block_lc = 0;  /* block-wise line count */
    size_t total_lc = 0;  /* total line count */
    samrecord_t* samrecord = &(fileenc->samparser->curr);

    /* Parse SAM file line by line and invoke encoders */
    while (samparser_next(fileenc->samparser)) {
        if (block_lc >= fileenc->block_sz) {
            /*
             * Write block header:
             * - 4 bytes: block count
             * - 4 bytes: no. of lines in block
             */
            tsc_log("Writing block %zu: %zu line(s)\n", block_cnt, block_lc);
            total_bc += fwrite_uint32(fileenc->ofp, (uint32_t)block_cnt);
            total_bc += fwrite_uint32(fileenc->ofp, (uint32_t)block_lc);

            /* Write seq, qual, and aux blocks */
            size_t seq_bc = seqenc_write_block(fileenc->seqenc, fileenc->ofp);
            size_t qual_bc = qualenc_write_block(fileenc->qualenc, fileenc->ofp);
            size_t aux_bc = auxenc_write_block(fileenc->auxenc, fileenc->ofp);
            total_bc += seq_bc;
            total_bc += qual_bc;
            total_bc += aux_bc;
            total_seq_bc += seq_bc;
            total_qual_bc += qual_bc;
            total_aux_bc += aux_bc;

            block_cnt++;
            block_lc = 0;
        }

        /* Add records to encoders */
        seqenc_add_record(fileenc->seqenc,
                          samrecord->int_fields[POS],
                          samrecord->str_fields[CIGAR],
                          samrecord->str_fields[SEQ]);
        qualenc_add_record(fileenc->qualenc,
                           samrecord->str_fields[QUAL]);
        auxenc_add_record(fileenc->auxenc,
                          samrecord->str_fields[QNAME],
                          samrecord->int_fields[FLAG],
                          samrecord->str_fields[RNAME],
                          samrecord->int_fields[MAPQ],
                          samrecord->str_fields[RNEXT],
                          samrecord->int_fields[PNEXT],
                          samrecord->int_fields[TLEN],
                          samrecord->str_fields[OPT]);
        block_lc++;
        total_lc++;
    }

    /*
     * Write last block header:
     * - 4 bytes: block count
     * - 4 bytes: no. of lines in block
     */
    tsc_log("Writing last block %zu: %zu line(s)\n", block_cnt, block_lc);
    total_bc += fwrite_uint32(fileenc->ofp, (uint32_t)block_cnt);
    total_bc += fwrite_uint32(fileenc->ofp, (uint32_t)block_lc);

    /* Write last seq, qual, and aux blocks */
    size_t seq_bc = seqenc_write_block(fileenc->seqenc, fileenc->ofp);
    size_t qual_bc = qualenc_write_block(fileenc->qualenc, fileenc->ofp);
    size_t aux_bc = auxenc_write_block(fileenc->auxenc, fileenc->ofp);
    total_bc += seq_bc;
    total_bc += qual_bc;
    total_bc += aux_bc;
    total_seq_bc += seq_bc;
    total_qual_bc += qual_bc;
    total_aux_bc += aux_bc;

    block_cnt++;
    
    tsc_log("Wrote %zu bytes ~= %.2f GiB (%zu line(s) in %zu block(s))\n", total_bc, ((double)total_bc / GB), total_lc, block_cnt);

    /* Return compression statistics */
    char stats[4*KB];
    snprintf(stats, sizeof(stats), "  Compression Statistics:\n"
                                   "  -----------------------\n"
                                   "  SAM records (lines) processed: %zu\n"
                                   "  Number of blocks written     : %zu\n"
                                   "  Total bytes written          : %zu\n"
                                   "  Total SEQ- bytes written     : %zu\n"
                                   "  Total QUAL bytes written     : %zu\n"
                                   "  Total AUX- bytes written     : %zu\n"
    , total_lc, block_cnt, total_bc, total_seq_bc, total_qual_bc, total_aux_bc);
    str_append_cstr(fileenc->stats, stats);

    return fileenc->stats;
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
    filedec_t* filedec = (filedec_t *)tsc_malloc_or_die(sizeof(filedec_t));
    filedec->seqdec = seqdec_new();
    filedec->qualdec = qualdec_new();
    filedec->auxdec = auxdec_new();
    filedec->stats = str_new();
    filedec_init(filedec, ifp, ofp);
    return filedec;
}

void filedec_free(filedec_t* filedec)
{
    if (filedec != NULL) {
        seqdec_free(filedec->seqdec);
        qualdec_free(filedec->qualdec);
        auxdec_free(filedec->auxdec);
        str_free(filedec->stats);
        free((void*)filedec);
        filedec = NULL;
    } else { /* filedec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

str_t* filedec_decode(filedec_t* filedec)
{
    /*
     * Read tsc file header:
     * - 4 bytes: 'TSC' + 0x00
     * - 4 bytes: version + 0x00
     * - 4 bytes: block size used for encoding
     */
    unsigned char tmp1[4], tmp2[4];
    if (fread_buf(filedec->ifp, tmp1, 4) != 4)
        tsc_error("Failed to read file header!\n");
    if (fread_buf(filedec->ifp, tmp2, 4) != 4)
        tsc_error("Failed to read file header!\n");
    tsc_log("Format: %s %s\n", tmp1, tmp2);
    uint32_t block_sz;
    if (fread_uint32(filedec->ifp, &block_sz) != sizeof(uint32_t))
        tsc_error("Failed to read block size!\n");
    tsc_log("Block size: %d\n", block_sz);

    /*
     * Read SAM header:
     * - 4 bytes: number of bytes in header
     * - n bytes: SAM header
     */
    tsc_log("Reading SAM header\n");
    uint32_t header_bc;
    if (fread_uint32(filedec->ifp, &header_bc) != sizeof(uint32_t))
        tsc_error("Failed to read SAM header size!\n");
    str_t* header = str_new();
    str_reserve(header, header_bc);
    if (fread_buf(filedec->ifp, (unsigned char*)header->s, header_bc) != header_bc)
        tsc_error("Failed to read SAM header!\n");
    fwrite_cstr(filedec->ofp, header->s);
    str_free(header);

    /* Prepare decoding */
    uint32_t block_cnt = 0;
    uint32_t block_lc = 0;

    while (fread_uint32(filedec->ifp, &block_cnt) == sizeof(uint32_t)) {
        /*
         * Read block header:
         * - 4 bytes: block count
         * - 4 bytes: lines in block
         */
        if (fread_uint32(filedec->ifp, &block_lc) != sizeof(uint32_t))
            tsc_error("Failed to read number of lines in block %d!\n", block_cnt);
        tsc_log("Decoding block %zu: %zu lines\n", block_cnt, block_lc);

        /* Allocate memory to prepare decoding of the next block */
        str_t** seq = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * block_lc);
        str_t** qual = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * block_lc);
        str_t** aux = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * block_lc);
        
        size_t i = 0;
        for (i = 0; i < block_lc; i++) {
            seq[i] = str_new();
            qual[i] = str_new();
            aux[i] = str_new();
        }

        /* Decode seq, qual, and aux blocks */
        seqdec_decode_block(filedec->seqdec, filedec->ifp, seq);
        qualdec_decode_block(filedec->qualdec, filedec->ifp, qual);
        auxdec_decode_block(filedec->auxdec, filedec->ifp, aux);

        /* TODO: Write seq, qual and aux IN CORRECT ORDER to filedec->ofp */
        tsc_log("Writing decoded block %zu: %zu lines\n", block_cnt, block_lc);
        for (i = 0; i < block_lc; i++) {
            fwrite_cstr(filedec->ofp, seq[i]->s);
            fwrite_cstr(filedec->ofp, qual[i]->s);
            fwrite_cstr(filedec->ofp, aux[i]->s);
        }

        /* Free the memory used for decoding */
        for (i = 0; i < block_lc; i++) {
            str_free(seq[i]);
            str_free(qual[i]);
            str_free(aux[i]);
        }
        free((void*)seq);
        free((void*)qual);
        free((void*)aux);
    }

    /* Return decompression statistics */
    char stats[4*KB];
    snprintf(stats, sizeof(stats), "  Decompression Statistics:\n");
    snprintf(stats, sizeof(stats), "  -------------------------\n");
    str_append_cstr(filedec->stats, stats);

    return filedec->stats;
}

