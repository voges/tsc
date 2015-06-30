/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

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
        tsc_log("Do you want to remove %s (y/n)? ", tsc_out_fname->s);
        if (tsc_yesno()) {
            unlink((const char*)tsc_out_fname->s);
            tsc_log("Removed %s\n", tsc_out_fname->s);
        }
    }
}

void tsc_abort(void)
{
    tsc_cleanup();
    exit(EXIT_FAILURE);
}

void tsc_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stderr, "%s: error: %s", tsc_prog_name->s, msg);
    free(msg);
    tsc_abort();
}

void tsc_warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stderr, "%s: warning: %s", tsc_prog_name->s, msg);
    free(msg);
}

void tsc_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stdout, "%s: %s", tsc_prog_name->s, msg);
    free(msg);
}

void* tsc_malloc(const size_t n)
{
    void* p = malloc(n);
    if (p == NULL) tsc_error("Cannot allocate %zu bytes.\n", n);
    return p;
}

void* tsc_realloc(void* ptr, const size_t n)
{
    void* p = realloc(ptr, n);
    if (p == NULL) tsc_error("Cannot allocate %zu bytes.\n", n);
    return p;
}

FILE* tsc_fopen(const char* fname, const char* mode)
{
    FILE *fp = fopen(fname, mode);
    if (fp == NULL) {
        fclose(fp);
        tsc_error("Failed to open file: %s\n", fname);
    }
    return fp;
}

void tsc_fclose(FILE* fp)
{
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    } else {
        tsc_error("Failed to close file.\n");
    }
}

