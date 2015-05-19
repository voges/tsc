/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_FWRITER_H
#define TSC_FWRITER_H

#include <stdio.h>
#include <stdbool.h>
#include "tsclib.h"

typedef struct fwriter_t_ {
    FILE*        fp;              /* associated stream */
    char         bytebuf[4 * KB]; /* byte buffer       */
    unsigned int bytebuf_cnt;
    unsigned int bytebuf_pos;
} fwriter_t;

fwriter_t* fwriter_new(FILE* fp);
void fwriter_free(fwriter_t* fwriter);
bool fwriter_write_byte(fwriter_t* fwriter, const char byte);
void fwriter_write_flush(fwriter_t* fwriter);

#endif /* TSC_FWRITER_H */

