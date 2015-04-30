/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "samrecord.h"
#include "tsclib.h"
#include <string.h>

static void samrecord_init(samrecord_t* samrecord)
{
    memset(samrecord->line, 0, 8 * MB);
    int i = 0;
    for (i = 0; i < 7; i++) { samrecord->str_fields[i] = NULL; }
    for (i = 0; i < 5; i++) { samrecord->int_fields[i] = 0; }
}

samrecord_t* samrecord_new(void)
{
    samrecord_t* samrecord = (samrecord_t*)tsc_malloc_or_die(sizeof(samrecord_t));
    samrecord_init(samrecord);
    return samrecord;
}

void samrecord_free(samrecord_t* samrecord)
{
    if (samrecord) {
        free(samrecord);
        samrecord = NULL;
    } else { /* samrecord == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void samrecord_clear(samrecord_t* samrecord)
{
    if (samrecord) {
        samrecord_init(samrecord);
    } else { /* samrecord == NULL */
        tsc_error("Tried to work on NULL pointer. Aborting.");
    }
}

void samrecord_print(samrecord_t* samrecord)
{
    tsc_log("%s %d %s %d %d %s %s %d %s %d %s %s %s",
            samrecord->str_fields[QNAME],
            samrecord->int_fields[FLAG],
            samrecord->str_fields[RNAME],
            samrecord->int_fields[POS],
            samrecord->int_fields[MAPQ],
            samrecord->str_fields[CIGAR],
            samrecord->str_fields[RNEXT],
            samrecord->int_fields[PNEXT],
            samrecord->int_fields[TLEN],
            samrecord->str_fields[SEQ],
            samrecord->str_fields[QUAL],
            samrecord->str_fields[OPT]
           );
}
