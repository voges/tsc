/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tntlib.                                               *
 ******************************************************************************/

#include "tntlib_common.h"
#include <stdarg.h>
#include <stdbool.h>

static bool tnt_yesno(void)
{
    int c = getchar();
    bool yes = c == 'y' || c == 'Y';
    while (c != '\n' && c != EOF) c = getchar();
    return yes;
}

static void tnt_cleanup(void)
{

}

void tnt_abort(void)
{
    tnt_cleanup();
    exit(EXIT_FAILURE);
}

void tnt_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stderr, "Error: %s", msg);
    free(msg);
    tnt_abort();
}

void tnt_warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stderr, "Warning: %s", msg);
    free(msg);
}

void tnt_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stdout, "%s", msg);
    free(msg);
}

void* tnt_malloc(const size_t n)
{
    void* p = malloc(n);
    if (p == NULL) tnt_error("Cannot allocate %zu bytes.\n", n);
    return p;
}

void* tnt_realloc(void* ptr, const size_t n)
{
    void* p = realloc(ptr, n);
    if (p == NULL) tnt_error("Cannot allocate %zu bytes.\n", n);
    return p;
}

FILE* tnt_fopen(const char* fname, const char* mode)
{
    FILE *fp = fopen(fname, mode);
    if (fp == NULL) {
        fclose(fp);
        tnt_error("Failed to open file: %s\n", fname);
    }
    return fp;
}

void tnt_fclose(FILE* fp)
{
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    } else {
        tnt_error("Failed to close file.\n");
    }
}

