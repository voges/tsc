/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "tsclib.h"
#include <stdarg.h>
#include <unistd.h>

void tsc_cleanup(void)
{
    if (tsc_in_fd != NULL) {
        tsc_fclose_or_die(tsc_in_fd);
    }
    if (tsc_out_fd != NULL) {
        tsc_fclose_or_die(tsc_out_fd);
    }
    if (tsc_out_fname->s) {
        unlink((const char*)tsc_out_fname->s);
        tsc_log("Removed: %s", tsc_out_fname->s);
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
    fprintf(stderr, "%s error: %s\n", tsc_prog_name->s, msg);
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
    fprintf(stderr, "%s warning: %s\n", tsc_prog_name->s, msg);
    free(msg);
}

void tsc_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stdout, "%s: %s\n", tsc_prog_name->s, msg);
    free(msg);
}

void* tsc_malloc_or_die(const size_t n)
{
    void* p = malloc(n);
    if (p == NULL) {
        tsc_error("Cannot allocate %zu bytes.", n);
    }
    return p;
}

void* tsc_realloc_or_die(void* ptr, const size_t n)
{
    void* p = realloc(ptr, n);
    if (p == NULL) {
        tsc_error("Cannot allocate %zu bytes.", n);
    }
    return p;
}

FILE* tsc_fopen_or_die(const char* fname, const char* mode)
{
    FILE *fp = fopen(fname, mode);
    if (fp == NULL) {
        fclose(fp);
        tsc_error("Error opening file: %s", fname);
    }
    return fp;
}

void tsc_fclose_or_die(FILE* fp)
{
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
    else {
        tsc_error("Error closing file.");
        exit(EXIT_FAILURE);
    }
}
