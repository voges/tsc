/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_FILEENC_H
#define TSC_FILEENC_H

#include <stdio.h>
#include "frwb.h"
#include "samparser.h"

typedef struct fileenc_t_ {
    FILE* ifp;
    FILE* ofp;
    frwb_t* frwb;
    samparser_t* samparser;
} fileenc_t;

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp);
void fileenc_free(fileenc_t* fileenc);
void fileenc_encode(fileenc_t* fileenc);

#endif // TSC_FILEENC_H

