#define _GNU_SOURCE

#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void tsc_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stdout, "%s", msg);
    free(msg);
}

void tsc_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stdout, "Error: %s", msg);
    free(msg);
    exit(EXIT_FAILURE);
}
