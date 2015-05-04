/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "frwb.h"
#include <string.h>
#include <stdlib.h>

static void frwb_init(frwb_t* frwb, FILE* fp)
{
    frwb->fp = fp;
    memset(frwb->bytebuf, 0x0000, sizeof(frwb->bytebuf));
    frwb->bytebuf_cnt = 0;
    frwb->bytebuf_pos = 0;
}

frwb_t* frwb_new(FILE* fp)
{
    frwb_t* frwb = (frwb_t*)tsc_malloc_or_die(sizeof(frwb_t));
    frwb_init(frwb, fp);
    return frwb;
}

void frwb_free(frwb_t* frwb)
{
    if (frwb != NULL) {
        free((void*)frwb);
        frwb = NULL;
    } else { /* frwb == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

bool frwb_read_byte(frwb_t* frwb, char* byte)
{
    /* TODO: Need to test this one first! */
    if (frwb->bytebuf_pos == frwb->bytebuf_cnt) {
        /* Buffer has already been read out or is empty. (Re-)fill it. */
        frwb->bytebuf_cnt = fread(frwb->bytebuf, 1, sizeof(frwb->bytebuf), frwb->fp);
        frwb->bytebuf_pos = 0;
    }

    /* If there is at least one byte in the buffer, return it. */
    if (frwb->bytebuf_pos < frwb->bytebuf_cnt) {
        *byte = frwb->bytebuf[frwb->bytebuf_pos++];
        return true;
    }

    return false;
}

bool frwb_write_byte(frwb_t* frwb, const char byte)
{

    frwb->bytebuf[frwb->bytebuf_cnt++] = byte;

    if (frwb->bytebuf_cnt == sizeof(frwb->bytebuf)) {
        // Byte buffer is full: write everything.
        frwb->bytebuf_pos = 0;
        frwb->bytebuf_cnt = 0;
        size_t n = fwrite(frwb->bytebuf, 1, sizeof(frwb->bytebuf), frwb->fp);
        if (n < sizeof(frwb->bytebuf)) {
            tsc_warning("Could only write %d/%d bytes!", n, sizeof(frwb->bytebuf));
        }
    }

    return true;
}

void frwb_write_flush(frwb_t* frwb)
{
    fwrite(frwb->bytebuf, 1, frwb->bytebuf_cnt, frwb->fp);

}

