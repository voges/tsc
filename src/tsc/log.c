#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void tsc_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *msg;
    int rc = vasprintf(&msg, fmt, args);
    if (rc == -1) {
        fprintf(stderr, "tsc: error: Error executing vasprintf\n");
        exit(EXIT_FAILURE);
    }
    va_end(args);
    fprintf(stdout, "%s", msg);
    free(msg);
}

void tsc_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *msg;
    int rc = vasprintf(&msg, fmt, args);
    if (rc == -1) {
        fprintf(stderr, "tsc: error: Error executing vasprintf\n");
        exit(EXIT_FAILURE);
    }
    va_end(args);
    fprintf(stderr, "tsc: error: %s", msg);
    free(msg);
    exit(EXIT_FAILURE);
}
