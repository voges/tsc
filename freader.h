/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_FREADER_H
#define TSC_FREADER_H

#include <stdio.h>
#include <stdbool.h>
#include "tsclib.h"

typedef struct freader_t_ {
    FILE*        fp;              /* associated stream   */
    char         bytebuf[4 * KB]; /* byte buffer         */
    unsigned int bytebuf_cnt;
    unsigned int bytebuf_pos;
} freader_t;

freader_t* freader_new(FILE* fp);
void freader_free(freader_t* freader);
bool freader_read_byte(freader_t* freader, char* byte);

#endif /* TSC_FREADER_H */

