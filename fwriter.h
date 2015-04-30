/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_FWRITER_H
#define TSC_FWRITER_H

#include <stdint.h>
#include <stdio.h>
#include "tsclib.h"

typedef struct fwriter_t_ {
    FILE*        fp;                /* stream to write to      */
    uint8_t      bytebuf[4 * KB];   /* byte buffer */
    unsigned int bytebuf_cnt;       /* byte buffer counter     */
    uint32_t     bitbuf;            /* bit buffer              */
    unsigned int bitbuf_cnt;        /* bit buffer counter      */
} fwriter_t;

fwriter_t* fwriter_new(FILE* fp);
void fwriter_free(fwriter_t* fwriter);
void fwriter_write_bits(fwriter_t* fwriter, const uint32_t bits, const unsigned int n);
void fwriter_write_byte(fwriter_t* fwriter, const uint8_t byte);
void fwriter_write_uint8(fwriter_t* fwriter, const uint8_t byte);
void fwriter_write_uint32(fwriter_t* fwriter, const uint32_t quadword);

#endif /* TSC_FWRITER_H */
