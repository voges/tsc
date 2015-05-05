/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_FRWB_H
#define TSC_FRWB_H

/*
 * Buffered binary in-/output
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "tsclib.h"

typedef struct frwb_t_ {
    FILE*        fp;              /* associated stream   */
    char         bytebuf[4 * KB]; /* byte buffer         */
    unsigned int bytebuf_cnt;
    unsigned int bytebuf_pos;
} frwb_t;

frwb_t* frwb_new(FILE* fp);
void frwb_free(frwb_t* frwb);
bool frwb_read_byte(frwb_t* frwb, char* byte);
bool frwb_write_byte(frwb_t* frwb, const char byte);
void frwb_write_flush(frwb_t* frwb);

#endif /* TSC_FRWB_H */
