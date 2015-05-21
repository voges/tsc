/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "freader.h"
#include <string.h>

static void freader_init(freader_t* freader, FILE* fp)
{
    freader->fp = fp;
    memset(freader->bytebuf, 0x0000, sizeof(freader->bytebuf));
    freader->bytebuf_cnt = 0;
    freader->bytebuf_pos = 0;
}

freader_t* freader_new(FILE* fp)
{
    freader_t* freader = (freader_t*)tsc_malloc_or_die(sizeof(freader_t));
    freader_init(freader, fp);
    return freader;
}

void freader_free(freader_t* freader)
{
    if (freader != NULL) {
        free((void*)freader);
        freader = NULL;
    } else { /* freader == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

bool freader_read_byte(freader_t* freader, char* byte)
{
    if (freader->bytebuf_pos == freader->bytebuf_cnt) {
        /* Buffer has already been read out or is empty. (Re-)fill it. */
        freader->bytebuf_cnt = fread(freader->bytebuf, 1, sizeof(freader->bytebuf), freader->fp);
        if (freader->bytebuf_cnt == 0) {
            return false; /* Reached EOF */
        }
        freader->bytebuf_pos = 0;
    }

    /* We have at least one byte in the buffer, return it. */
    *byte = freader->bytebuf[freader->bytebuf_pos++];
    return true;
}

