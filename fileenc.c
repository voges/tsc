/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "fileenc.h"
#include "tsclib.h"
#include <string.h>

static void fileenc_init(fileenc_t* fileenc, FILE* ifp, FILE* ofp)
{
    fileenc->ifp = ifp;
    fileenc->ofp = ofp;
}

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp)
{
    fileenc_t* fileenc = (fileenc_t *)tsc_malloc_or_die(sizeof(fileenc_t));
    fileenc->block_sz = 100;
    fileenc->frwb = frwb_new(ofp);
    fileenc->samparser = samparser_new(ifp);
    fileenc->seqenc = seqenc_new(fileenc->block_sz);
    fileenc->qualenc = qualenc_new(fileenc->block_sz);
    fileenc->auxenc = auxenc_new(fileenc->block_sz);
    fileenc_init(fileenc, ifp, ofp);
    return fileenc;
}

void fileenc_free(fileenc_t* fileenc)
{
    if (fileenc != NULL) {
        frwb_free(fileenc->frwb);
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

static void fileenc_write_string(fileenc_t* fileenc, const char* string)
{
    size_t nbytes = strlen(string);
    unsigned int i = 0;
    for (i = 0; i < nbytes; i++) {
        frwb_write_byte(fileenc->frwb, *string++);
    }
}

static void fileenc_write_int64(fileenc_t* fileenc, int64_t integer)
{
    size_t nbytes = sizeof(integer);
    unsigned int i = 0;
    for (i = 0; i < nbytes; i++) {
        frwb_write_byte(fileenc->frwb, (integer >> i*8) & 0xFF);
    }
}

void fileenc_encode(fileenc_t* fileenc)
{
    unsigned int block_cnt = 0;
    unsigned int line_cnt = 0;

    /* Write SAM header */
    //fileenc_write_string(fileenc, fileenc->samparser->head->s);

    samrecord_t* samrecord = &(fileenc->samparser->curr);

    samparser_next(fileenc->samparser);
    seqenc_add_record(fileenc->seqenc, samrecord->int_fields[POS], samrecord->str_fields[CIGAR], samrecord->str_fields[SEQ]);
    qualenc_add_record(fileenc->qualenc, samrecord->str_fields[QUAL]);
    auxenc_add_record(fileenc->auxenc, samrecord->str_fields[RNAME]);

    while (samparser_next(fileenc->samparser)) {

        if (line_cnt == fileenc->block_sz - 1) {
            DEBUG("Writing block ...");
            str_t* out = str_new();
            seqenc_output_records(fileenc->seqenc, out);
            fileenc_write_string(fileenc, out->s);
            //qualenc_output_records(fileenc->qualenc, fileenc->frwb);
            //auxenc_output_records(fileenc->auxenc, fileenc->frwb);
            str_free(out);
            line_cnt = 0;
        }

        seqenc_add_record(fileenc->seqenc, samrecord->int_fields[POS], samrecord->str_fields[CIGAR], samrecord->str_fields[SEQ]);
        qualenc_add_record(fileenc->qualenc, samrecord->str_fields[QUAL]);
        auxenc_add_record(fileenc->auxenc, samrecord->str_fields[RNAME]);
        line_cnt++;

    }

    frwb_write_flush(fileenc->frwb);
}

