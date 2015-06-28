/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

/*
 * TODO: Add DeeZ copyright.
 */

#include "samparser.h"
#include <string.h>

static void samparser_init(samparser_t* sp, FILE* fp)
{
    sp->fp = fp;

    /* Read the SAM header. */
    while (fgets(sp->curr.line, sizeof(sp->curr.line), sp->fp)) {
        if (*(sp->curr.line) == '@') {
            str_append_cstr(sp->head, sp->curr.line);
        } else {
            long offset = -strlen(sp->curr.line);
            fseek(sp->fp, offset, SEEK_CUR);
            break;
        }
    }
}

samparser_t* samparser_new(FILE* fp)
{
    samparser_t* sp = (samparser_t*)tsc_malloc(sizeof(samparser_t));
    sp->head = str_new();
    samparser_init(sp, fp);
    return sp;
}

void samparser_free(samparser_t* sp)
{
    if (sp != NULL) {
        str_free(sp->head);
        free(sp);
        sp = NULL;
    } else { /* sp == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void samparser_parse(samparser_t* sp)
{
    size_t l = strlen(sp->curr.line) - 1;

    while (l && (sp->curr.line[l] == '\r' || sp->curr.line[l] == '\n'))
        sp->curr.line[l--] = '\0';

    char* c = sp->curr.str_fields[0] = sp->curr.line;
    int f = 1, sfc = 1, ifc = 0;

    while (*c) {
        if (*c == '\t') {
            if (f == 1 || f == 3 || f == 4 || f == 7 || f == 8)
                sp->curr.int_fields[ifc++] = atoi(c + 1);
            else
                sp->curr.str_fields[sfc++] = c + 1;
            f++;
            *c = '\0';
            if (f == 12) { break; }
        }
        c++;
    }

    if (f == 11) sp->curr.str_fields[sfc++] = c;
}

bool samparser_next(samparser_t* sp)
{
    /* Try to read and parse next line. */
    if (fgets(sp->curr.line, sizeof(sp->curr.line), sp->fp)) {
        if (*(sp->curr.line) == '@')
            tsc_error("Tried to read SAM record but found header line.\n");
        else
            samparser_parse(sp);
    } else {
        return false;
    }
    return true;
}

