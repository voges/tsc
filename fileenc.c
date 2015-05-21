/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "fileenc.h"
#include "tsclib.h"
#include <string.h>

static void fileenc_init(fileenc_t* fileenc, FILE* ifp, FILE* ofp, const unsigned int block_sz)
{
    fileenc->ifp = ifp;
    fileenc->ofp = ofp;
    fileenc->block_sz = block_sz;
}

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp, const unsigned int block_sz)
{
    fileenc_t* fileenc = (fileenc_t *)tsc_malloc_or_die(sizeof(fileenc_t));
    fileenc->fwriter = fwriter_new(ofp);
    fileenc->samparser = samparser_new(ifp);
    fileenc->seqenc = seqenc_new(block_sz);
    fileenc->qualenc = qualenc_new(block_sz, QUALENC_WINDOW_SZ);
    fileenc->auxenc = auxenc_new(block_sz);
    fileenc_init(fileenc, ifp, ofp, block_sz);
    return fileenc;
}

void fileenc_free(fileenc_t* fileenc)
{
    if (fileenc != NULL) {
        fwriter_free(fileenc->fwriter);
        samparser_free(fileenc->samparser);
        seqenc_free(fileenc->seqenc);
        qualenc_free(fileenc->qualenc);
        auxenc_free(fileenc->auxenc);
        free((void*)fileenc);
        fileenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void fileenc_encode(fileenc_t* fileenc)
{
    unsigned int block_cnt = 0;
    unsigned int block_lc = 0;

    /* Write SAM header */
    fwriter_write_cstr(fileenc->fwriter, fileenc->samparser->head->s);

    /* Structure holding the SAM records */
    samrecord_t* samrecord = &(fileenc->samparser->curr);

    /* Parse SAM file line by line and invoke encoders */
    while (samparser_next(fileenc->samparser)) {
        if (block_lc >= fileenc->block_sz) {
            DEBUG("Writing block %d with %d lines.", block_cnt, block_lc);
            seqenc_write_block(fileenc->seqenc, fileenc->fwriter);
            //qualenc_write_block(fileenc->qualenc, fileenc->fwriter);
            //auxenc_write_block(fileenc->auxenc, fileenc->fwriter);
            block_cnt++;
            block_lc = 0;
        }

        DEBUG("Block: %d, Line: %d: Adding records to encoders.", block_cnt, block_lc);
        seqenc_add_record(fileenc->seqenc, samrecord->int_fields[POS], samrecord->str_fields[CIGAR], samrecord->str_fields[SEQ]);
        qualenc_add_record(fileenc->qualenc, samrecord->str_fields[QUAL]);
        auxenc_add_record(fileenc->auxenc, samrecord->str_fields[RNAME]);
        block_lc++;
    }

    DEBUG("Writing block %d with %d lines.", block_cnt, block_lc);
    seqenc_write_block(fileenc->seqenc, fileenc->fwriter);
    //qualenc_write_block(fileenc->qualenc, fileenc->fwriter);
    //auxenc_write_block(fileenc->auxenc, fileenc->fwriter);

    fwriter_write_flush(fileenc->fwriter);
}

