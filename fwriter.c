/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                               *
 *****************************************************************************/

#include "fwriter.h"
#include <string.h>
#include <stdlib.h>

static void fwriter_init(fwriter_t* fwriter, FILE* fp)
{
    fwriter->fp = fp;
    memset(fwriter->bytebuf, 0, fwriter->bytebuf_cnt);
    fwriter->bytebuf_cnt = 0;
    fwriter->bitbuf = 0x0000;
    fwriter->bitbuf_cnt = 0;
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
        free(fwriter);
        fwriter = NULL;
    }
    else { /* fwriter == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

static void fwriter_fwrite(void* stream, const uint8_t* data, const size_t nbytes)
{
    fwrite(data, 1, nbytes, (FILE*)stream);
}

void fwriter_write_bits(fwriter_t* fwriter, uint32_t bits, unsigned int nbits)
{
        /* Shift old bits to the left, add new to the right */
        fwriter->bitbuf = fwriter->bitbuf << nbits;
        fwriter->bitbuf |= bits & ((1 << nbits)-1);
        nbits += fwriter->bitbuf_cnt;

        /* Flush whole bytes */
        while (nbits >= 8) {
            uint8_t b;
            nbits -= 8;
            b = fwriter->bitbuf >> nbits;
            fwriter_write_uint8(fwriter, b);
        }

        fwriter->bitbuf = nbits;
}

void fwriter_write_byte(fwriter_t* fwriter, const uint8_t byte)
{
    fwriter_write_uint8(fwriter, byte);
}

void fwriter_write_uint8(fwriter_t* fwriter, const uint8_t byte)
{
    fwriter->bytebuf[fwriter->bytebuf_cnt++] = byte;

    if (fwriter->bytebuf_cnt == sizeof(fwriter->bytebuf)) {
        /* Byte buffer is full: write everything. */
        fwriter->bytebuf_cnt = 0;
        fwriter_fwrite(fwriter->fp, fwriter->bytebuf, sizeof(fwriter->bytebuf));
    }
}

void fwriter_write_uint32(fwriter_t* fwriter, const uint32_t quadword)
{

    fwriter_write_uint8(fwriter, (const uint8_t)(quadword >> 24));
    fwriter_write_uint8(fwriter, (const uint8_t)(quadword >> 16));
    fwriter_write_uint8(fwriter, (const uint8_t)(quadword >>  8));
    fwriter_write_uint8(fwriter, (const uint8_t)(quadword      ));
}
