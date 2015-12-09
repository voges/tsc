//
// Copyright (c) 2015
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
//

//
// This file is part of tsc.
//
// Tsc is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Tsc is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with tsc. If not, see <http://www.gnu.org/licenses/>
//

#include "tsclib.h"
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>

static bool tsc_yesno(void)
{
    int c = getchar();
    bool yes = c == 'y' || c == 'Y';
    while (c != '\n' && c != EOF)
        c = getchar();
    return yes;
}

void tsc_cleanup(void)
{
    if (tsc_in_fp != NULL)
        tsc_fclose(tsc_in_fp);
    if (tsc_out_fp != NULL)
        tsc_fclose(tsc_out_fp);
    if (tsc_out_fname->len > 0) {
        tsc_log(TSC_LOG_DEFAULT,
                "Do you want to remove %s (y/n)? ", tsc_out_fname->s);
        if (tsc_yesno()) {
            unlink((const char *)tsc_out_fname->s);
            tsc_log(TSC_LOG_DEFAULT, "Removed %s\n", tsc_out_fname->s);
        }
    }
}

void tsc_abort(void)
{
    tsc_cleanup();
    exit(EXIT_FAILURE);
}

void tsc_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stderr, "%s [error]: %s", tsc_prog_name->s, msg);
    free(msg);
    tsc_abort();
}

void tsc_log(tsc_loglvl_t loglvl, const char *fmt, ...)
{
    char *msg_head;

    switch (loglvl) {
    case TSC_LOG_DEFAULT:
        asprintf(&msg_head,"%s [log]: ", tsc_prog_name->s);
        break;
    case TSC_LOG_INFO:
        asprintf(&msg_head,"%s [info]: ", tsc_prog_name->s);
        break;
    case TSC_LOG_VERBOSE:
        asprintf(&msg_head,"%s [verbose]: ", tsc_prog_name->s);
        break;
    case TSC_LOG_WARN:
        asprintf(&msg_head,"%s [warn]: ", tsc_prog_name->s);
        break;
    default:
        fprintf(stderr, "%s [error]: Wrong log level", tsc_prog_name->s);
        tsc_abort();
    }

    if (loglvl <= tsc_loglvl) {
        va_list args;
        va_start(args, fmt);
        char *msg;
        vasprintf(&msg, fmt, args);
        va_end(args);
        fprintf(stdout, "%s%s", msg_head, msg);
        free(msg);
    }
}

void * tsc_malloc(const size_t n)
{
    void *p = malloc(n);
    if (p == NULL) tsc_error("Cannot allocate %zu bytes\n", n);
    return p;
}

void *tsc_realloc(void *ptr, const size_t n)
{
    void *p = realloc(ptr, n);
    if (p == NULL) tsc_error("Cannot allocate %zu bytes\n", n);
    return p;
}

FILE * tsc_fopen(const char *fname, const char *mode)
{
    FILE *fp = fopen(fname, mode);
    if (fp == NULL) {
        fclose(fp);
        tsc_error("Failed to open file: %s\n", fname);
    }
    return fp;
}

void tsc_fclose(FILE *fp)
{
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    } else {
        tsc_error("Failed to close file\n");
    }
}

