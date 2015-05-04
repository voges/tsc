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
    fileenc->frwb = frwb_new(ofp);
    fileenc->samparser = samparser_new(ifp);
    fileenc_init(fileenc, ifp, ofp);
    return fileenc;
}

void fileenc_free(fileenc_t* fileenc)
{
    if (fileenc != NULL) {
        frwb_free(fileenc->frwb);
        samparser_free(fileenc->samparser);
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
    /* Write SAM header */
    fileenc_write_string(fileenc, fileenc->samparser->head->s);

    samrecord_t* samrecord = &(fileenc->samparser->curr);

    while (samparser_next(fileenc->samparser)) {
        fileenc_write_string(fileenc, samrecord->str_fields[QNAME]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_int64(fileenc, samrecord->int_fields[FLAG]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_string(fileenc, samrecord->str_fields[RNAME]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_int64(fileenc, samrecord->int_fields[POS]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_int64(fileenc, samrecord->int_fields[MAPQ]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_string(fileenc, samrecord->str_fields[CIGAR]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_string(fileenc, samrecord->str_fields[RNEXT]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_int64(fileenc, samrecord->int_fields[PNEXT]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_int64(fileenc, samrecord->int_fields[TLEN]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_string(fileenc, samrecord->str_fields[SEQ]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_string(fileenc, samrecord->str_fields[QUAL]);
        fileenc_write_string(fileenc, "\t");
        fileenc_write_string(fileenc, samrecord->str_fields[OPT]);
        fileenc_write_string(fileenc, "\n");
    }

    frwb_write_flush(fileenc->frwb);
}

