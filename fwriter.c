/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "fwriter.h"
#include <string.h>

static void fwriter_init(fwriter_t* fwriter, FILE* fp)
{
    fwriter->fp = fp;
    memset(fwriter->bytebuf, 0x0000, sizeof(fwriter->bytebuf));
    fwriter->bytebuf_cnt = 0;
    fwriter->bytebuf_pos = 0;
}

fwriter_t* fwriter_new(FILE* fp)
{
    fwriter_t* fwriter = (fwriter_t*)tsc_malloc_or_die(sizeof(fwriter_t));
    fwriter_init(fwriter, fp);
    return fwriter;
}

void fwriter_free(fwriter_t* fwriter)
{
    if (fwriter != NULL) {
        free((void*)fwriter);
        fwriter = NULL;
    } else { /* fwriter == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void fwriter_write_byte(fwriter_t* fwriter, const char byte)
{
    fwriter->bytebuf[fwriter->bytebuf_cnt++] = byte;

    if (fwriter->bytebuf_cnt == sizeof(fwriter->bytebuf)) {
        // Byte buffer is full: write everything.
        fwriter->bytebuf_pos = 0;
        fwriter->bytebuf_cnt = 0;
        size_t n = fwrite(fwriter->bytebuf, 1, sizeof(fwriter->bytebuf), fwriter->fp);
        if (n < sizeof(fwriter->bytebuf)) {
            tsc_error("Could only write %d/%d bytes!", n, sizeof(fwriter->bytebuf));
        }
    }
}

void fwriter_write_uint64(fwriter_t* fwriter, uint64_t dword)
{
    unsigned int i = 7;
    do {
        fwriter_write_byte(fwriter, (dword >> (8 * i--)) & 0xF);
    } while (i > 0);
}

void fwriter_write_cstr(fwriter_t* fwriter, const char* cstr)
{
    size_t nbytes = strlen(cstr);
    unsigned int i = 0;
    for (i = 0; i < nbytes; i++) {
        fwriter_write_byte(fwriter, *cstr++);
    }
}

void fwriter_write_flush(fwriter_t* fwriter)
{
    fwrite(fwriter->bytebuf, 1, fwriter->bytebuf_cnt, fwriter->fp);
}

