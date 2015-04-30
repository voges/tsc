/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_FREADER_H
#define TSC_FREADER_H

#include <stdint.h>
#include <stdbool.h>
#include "tsclib.h"

typedef struct freader_t_ {
    FILE*        fp;              /**< stream to read from  */
    long         fsize;           /**< file size            */
    char         linebuf[4 * KB]; /**< line buffer          */
    uint8_t      bytebuf[4 * KB]; /**< byte buffer          */
    unsigned int bytebuf_cnt;     /**< byte buffer counter  */
    unsigned int bytebuf_pos;     /**< byte buffer position */
} freader_t;

freader_t* freader_new(FILE* fp);
void freader_free(freader_t* freader);
long freader_fsize(freader_t* freader);
long freader_fpos(freader_t* freader);
int freader_fseek(freader_t* freader, long offset);
bool freader_read_byte(freader_t* freader, uint8_t* byte);
char* freader_read_line(freader_t* freader);

#endif /* TSC_FREADER_H */
