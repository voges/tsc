/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "fileenc.h"
#include "tsclib.h"

static void fileenc_init(fileenc_t* fileenc, FILE* ifp, FILE* ofp)
{
    fileenc->ifp = ifp;
    fileenc->ofp = ofp;
}

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp)
{
    fileenc_t* fileenc = (fileenc_t *)tsc_malloc_or_die(sizeof(fileenc_t));
    //fileenc->ofwriter = fwriter_new(ofp);
    fileenc->samparser = samparser_new(ifp);
    fileenc_init(fileenc, ifp, ofp);
    return fileenc;
}

void fileenc_free(fileenc_t* fileenc)
{
    if (fileenc != NULL) {
        //fwriter_free(fileenc->ofwriter);
        samparser_free(fileenc->samparser);
        free((void*)fileenc);
        fileenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void fileenc_encode(fileenc_t* fileenc)
{
    samrecord_t* samrecord = samrecord_new();

    while (samparser_next(fileenc->samparser, samrecord)) {
        samrecord = fileenc->samparser->curr;
        DEBUG("%s", samrecord->str_fields[QUAL]);
        //samrecord_print(fileenc->samparser->curr);
    }
    DEBUG("Hello");
    samrecord_free(samrecord);
    DEBUG("Hello");
}
