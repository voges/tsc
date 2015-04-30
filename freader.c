/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "freader.h"
#include <stdio.h>
#include <string.h>
#include "tsclib.h"

static void freader_init(freader_t* freader, FILE* fp)
{
    freader->fp = fp;
    freader->fsize = 0L;
    memset(freader->linebuf, 0, sizeof(freader->linebuf));
    memset(freader->bytebuf, 0, sizeof(freader->bytebuf));
    freader->bytebuf_cnt = 0;
    freader->bytebuf_pos = 0;

    /* Compute file size */
    fseek(freader->fp, 0L, SEEK_END);
    freader->fsize = ftell(freader->fp);
    fseek(freader->fp, 0L, SEEK_SET);
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
        free(freader);
        freader = NULL;
    } else { /* freader == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

long freader_fsize(freader_t* freader)
{
    return freader->fsize;
}

long freader_fpos(freader_t* freader)
{
    return ftell(freader->fp);
}

int freader_fseek(freader_t* freader, long offset)
{
    return fseek(freader->fp, offset, SEEK_CUR);
}

static size_t freader_fread(FILE* fp, uint8_t* data, const size_t nbytes)
{
    return fread(data, 1, nbytes, fp);
}

bool freader_read_byte(freader_t* freader, uint8_t* byte)
{       
    if (freader->bytebuf_pos == freader->bytebuf_cnt) {
        /* Buffer has already been read out or is empty. (Re-)fill it. */
        freader->bytebuf_cnt = freader_fread(freader->fp, freader->bytebuf, sizeof(freader->bytebuf));
        freader->bytebuf_pos = 0;
    }

    /* If there is at least one byte in the buffer, return it. */
    if (freader->bytebuf_pos < freader->bytebuf_cnt) {
        *byte = freader->bytebuf[freader->bytebuf_pos++];
        return true;
    }

    return false;
}

char* freader_read_line(freader_t* freader)
{
    return fgets(freader->linebuf, sizeof(freader->linebuf), freader->fp);
}
