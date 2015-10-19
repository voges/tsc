/*
 * Copyright (c) 2015 
 * Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <voges@tnt.uni-hannover.de>
 */

/*
 * This file is part of gomp.
 *
 * Gomp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gomp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gomp. If not, see <http://www.gnu.org/licenses/>
 */

#include "gomplib.h"
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>

static bool gomp_yesno(void)
{
    int c = getchar();
    bool yes = c == 'y' || c == 'Y';
    while (c != '\n' && c != EOF)
        c = getchar();
    return yes;
}

void gomp_cleanup(void)
{
    if (gomp_in_fp != NULL)
        gomp_fclose(gomp_in_fp);
    if (gomp_out_fp != NULL)
        gomp_fclose(gomp_out_fp);
    if (gomp_out_fname->len > 0) {
        gomp_log("Do you want to remove %s (y/n)? ", gomp_out_fname->s);
        if (gomp_yesno()) {
            unlink((const char*)gomp_out_fname->s);
            gomp_log("Removed %s\n", gomp_out_fname->s);
        }
    }
}

void gomp_abort(void)
{
    gomp_cleanup();
    exit(EXIT_FAILURE);
}

void gomp_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stderr, "%s: error: %s", gomp_prog_name->s, msg);
    free(msg);
    gomp_abort();
}

void gomp_warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stderr, "%s: warning: %s", gomp_prog_name->s, msg);
    free(msg);
}

void gomp_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stdout, "%s: %s", gomp_prog_name->s, msg);
    free(msg);
}

void gomp_vlog(const char *fmt, ...)
{
    if (gomp_verbose) {
        va_list args;
        va_start(args, fmt);
        char *msg;
        vasprintf(&msg, fmt, args);
        va_end(args);
        fprintf(stdout, "%s: %s", gomp_prog_name->s, msg);
        free(msg);
    }
}

void * gomp_malloc(const size_t n)
{
    void *p = malloc(n);
    if (p == NULL) gomp_error("Cannot allocate %zu bytes!\n", n);
    return p;
}

void * gomp_realloc(void *ptr, const size_t n)
{
    void *p = realloc(ptr, n);
    if (p == NULL) gomp_error("Cannot allocate %zu bytes!\n", n);
    return p;
}

FILE * gomp_fopen(const char *fname, const char *mode)
{
    FILE *fp = fopen(fname, mode);
    if (fp == NULL) {
        fclose(fp);
        gomp_error("Failed to open file: %s\n", fname);
    }
    return fp;
}

void gomp_fclose(FILE *fp)
{
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    } else {
        gomp_error("Failed to close file!\n");
    }
}

