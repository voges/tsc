/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "samparser.h"
#include <string.h>

static void samparser_init(samparser_t* samparser, FILE* fp)
{
    samparser->fp = fp;

    /* Read the SAM header. */
    while (fgets(samparser->curr.line, sizeof(samparser->curr.line), samparser->fp)) {
        if (*(samparser->curr.line) == '@') {
            str_append_cstr(samparser->head, samparser->curr.line);
        } else {
            long offset = -strlen(samparser->curr.line);
            fseek(samparser->fp, offset, SEEK_CUR);
            break;
        }
    }
}

samparser_t* samparser_new(FILE* fp)
{
    samparser_t* samparser = (samparser_t*)tsc_malloc(sizeof(samparser_t));
    samparser->head = str_new();
    samparser_init(samparser, fp);
    return samparser;
}

void samparser_free(samparser_t* samparser)
{
    if (samparser != NULL) {
        str_free(samparser->head);
        free((void*)samparser);
        samparser = NULL;
    } else { /* samparser == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void samparser_parse(samparser_t* samparser)
{
    size_t l = strlen(samparser->curr.line) - 1;

    while (l && (samparser->curr.line[l] == '\r' || samparser->curr.line[l] == '\n')) {
        samparser->curr.line[l--] = '\0';
    }

    char* c = samparser->curr.str_fields[0] = samparser->curr.line;
    int f = 1, sfc = 1, ifc = 0;

    while (*c) {
        if (*c == '\t') {
            if (f == 1 || f == 3 || f == 4 || f == 7 || f == 8) {
                samparser->curr.int_fields[ifc++] = atoi(c + 1);
            } else {
                samparser->curr.str_fields[sfc++] = c + 1;
            }
            f++;
            *c = '\0';
            if (f == 12) { break; }
        }
        c++;
    }

    if (f == 11) samparser->curr.str_fields[sfc++] = c;
}

bool samparser_next(samparser_t* samparser)
{
    /* Try to read and parse next line. */
    if (fgets(samparser->curr.line, sizeof(samparser->curr.line), samparser->fp)) {
        if (*(samparser->curr.line) == '@')
            tsc_error("Tried to read SAM record but found header line.\n");
        else
            samparser_parse(samparser);
    } else {
        return false;
    }
    return true;
}

