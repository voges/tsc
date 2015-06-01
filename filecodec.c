/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "filecodec.h"
#include "tsclib.h"
#include "frw.h"
#include "version.h"
#include <string.h>

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
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
        free((void*)fileenc);
        fileenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.\n");
    }
}

void fileenc_encode(fileenc_t* fileenc)
{
    /*
     * Write tsc file header:
     * - 4 bytes: 'TSC' + 0x00
     * - 4 bytes: version + 0x00
     * - 4 bytes: block size used for encoding
     */
    tsc_log("Format: %s %s\n", tsc_prog_name->s, tsc_version->s);
    fwrite_cstr(fileenc->ofp, tsc_prog_name->s);
    fwrite_byte(fileenc->ofp, 0x00);
    fwrite_cstr(fileenc->ofp, tsc_version->s);
    fwrite_byte(fileenc->ofp, 0x00);
    tsc_log("Block size: %zu\n", fileenc->block_sz);
    fwrite_uint32(fileenc->ofp, fileenc->block_sz);

    /*
     * Write SAM header:
     * - 4 bytes: number of bytes in header
     * - n bytes: SAM header
     */
    tsc_log("Writing SAM header\n");
    fwrite_uint32(fileenc->ofp, fileenc->samparser->head->n);
    fwrite_cstr(fileenc->ofp, fileenc->samparser->head->s);

    /* Prepare encoding */
    size_t block_cnt = 0;
    size_t block_lc = 0;
    samrecord_t* samrecord = &(fileenc->samparser->curr);

    /* Parse SAM file line by line and invoke encoders */
    while (samparser_next(fileenc->samparser)) {
        if (block_lc >= fileenc->block_sz) {          
            /*
             * Write block header:
             * - 4 bytes: block count
             * - 4 bytes: lines in block
             */
            tsc_log("Writing block %zu: %zu lines\n", block_cnt, block_lc);
            fwrite_uint32(fileenc->ofp, (uint32_t)block_cnt);
            fwrite_uint32(fileenc->ofp, (uint32_t)block_lc);

            /*
             * Write seq, qual, and aux blocks:
             * - 4 bytes: identifier
             * - 8 bytes: number of bytes in block
             * - n bytes: encoded stream
             */
            fwrite_cstr(fileenc->ofp, "SEQ");
            fwrite_byte(fileenc->ofp, 0x00);
            fwrite_uint64(fileenc->ofp, fileenc->seqenc->block_b);
            seqenc_write_block(fileenc->seqenc, fileenc->ofp);

            fwrite_cstr(fileenc->ofp, "QUAL");
            fwrite_uint64(fileenc->ofp, fileenc->seqenc->block_b);
            qualenc_write_block(fileenc->qualenc, fileenc->ofp);

            fwrite_cstr(fileenc->ofp, "AUX");
            fwrite_byte(fileenc->ofp, 0x00);
            fwrite_uint64(fileenc->ofp, fileenc->auxenc->block_b);
            auxenc_write_block(fileenc->auxenc, fileenc->ofp);

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
    }

    /*
     * Write last block header:
     * - 4 bytes: block count
     * - 4 bytes: lines in block
     */
    tsc_log("Writing last block %zu: %zu lines\n", block_cnt, block_lc);
    fwrite_uint32(fileenc->ofp, (uint32_t)block_cnt);
    fwrite_uint32(fileenc->ofp, (uint32_t)block_lc);

    /*
     * Write last seq, qual, and aux blocks:
     * - 4 bytes: identifier
     * - 8 bytes: number of bytes in block
     * - n bytes: encoded stream
     */
    fwrite_cstr(fileenc->ofp, "SEQ");
    fwrite_byte(fileenc->ofp, 0x00);
    fwrite_uint64(fileenc->ofp, fileenc->seqenc->block_b);
    seqenc_write_block(fileenc->seqenc, fileenc->ofp);

    fwrite_cstr(fileenc->ofp, "QUAL");
    fwrite_uint64(fileenc->ofp, fileenc->seqenc->block_b);
    qualenc_write_block(fileenc->qualenc, fileenc->ofp);

    fwrite_cstr(fileenc->ofp, "AUX");
    fwrite_byte(fileenc->ofp, 0x00);
    fwrite_uint64(fileenc->ofp, fileenc->auxenc->block_b);
    auxenc_write_block(fileenc->auxenc, fileenc->ofp);
}

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
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
    filedec_init(filedec, ifp, ofp);
    return filedec;
}

void filedec_free(filedec_t* filedec)
{
    if (filedec != NULL) {
        seqdec_free(filedec->seqdec);
        qualdec_free(filedec->qualdec);
        auxdec_free(filedec->auxdec);
        free((void*)filedec);
        filedec = NULL;
    } else { /* filedec == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.\n");
    }
}

void filedec_decode(filedec_t* filedec)
{
    /*
     * Read tsc file header:
     * - 4 bytes: 'TSC' + 0x00
     * - 4 bytes: version + 0x00
     * - 4 bytes: block size used for encoding
     */
    unsigned char tmp1[4];
    unsigned char tmp2[4];
    fread_buf(filedec->ifp, tmp1, 4);
    fread_buf(filedec->ifp, tmp2, 4);
    tsc_log("Format: %s %s\n", tmp1, tmp2);
    uint32_t block_sz;
    fread_uint32(filedec->ifp, &block_sz);
    tsc_log("Block size: %d\n", block_sz);

    /*
     * Read SAM header:
     * - 4 bytes: number of bytes in header
     * - n bytes: SAM header
     */
    tsc_log("Reading SAM header\n");
    uint32_t header_b;
    fread_uint32(filedec->ifp, &header_b);
    str_t* header = str_new();
    str_reserve(header, header_b);
    fread_buf(filedec->ifp, (unsigned char*)header->s, header_b);
    fwrite_cstr(filedec->ofp, header->s);
    str_free(header);

    /* Prepare decoding */
    uint32_t block_cnt;
    uint32_t block_lc;
    unsigned char block_id[4];
    uint64_t block_b;

    while (fread_uint32(filedec->ifp, &block_cnt)) {
        /*
         * Read block header:
         * - 4 bytes: block count
         * - 4 bytes: lines in block
         */
        tsc_log("Reading block %zu: %zu lines\n", block_cnt, block_lc);
        fread_uint32(filedec->ifp, &block_cnt);
        fread_uint32(filedec->ifp, &block_lc);

        /*
         * Read seq, qual, and aux blocks:
         * - 4 bytes: identifier
         * - 8 bytes: number of bytes in block
         * - n bytes: encoded stream
         */
        fread_buf(filedec->ifp, block_id, 4);
        if (strncmp((char*)block_id, "SEQ", 3)) tsc_error("Wrong block order!\n");
        fread_uint64(filedec->ifp, &block_b);
        seqdec_decode_block(filedec->seqdec, block_b);

        fread_buf(filedec->ifp, block_id, 4);
        if (strncmp((char*)block_id, "QUAL", 4)) tsc_error("Wrong block order!\n");
        fread_uint64(filedec->ifp, &block_b);
        qualdec_decode_block(filedec->qualdec, block_b);

        fread_buf(filedec->ifp, block_id, 4);
        if (strncmp((char*)block_id, "AUX", 3)) tsc_error("Wrong block order!\n");
        fread_uint64(filedec->ifp, &block_b);
        auxdec_decode_block(filedec->auxdec, block_b);
    }
}

