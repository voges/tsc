// Copyright 2015 Leibniz University Hannover (LUH)

#include "mem.h"

#include "log.h"

void *tsc_malloc(const size_t size) {
    void *p = malloc(size);
    if (p == NULL) {
        tsc_error("Cannot allocate %zu bytes\n", size);
    }
    return p;
}

void *tsc_realloc(void *ptr, const size_t size) {
    void *p = realloc(ptr, size);
    if (p == NULL) {
        tsc_error("Cannot allocate %zu bytes\n", size);
    }
    return p;
}

void tsc_free(void *ptr) {
    if (ptr != NULL) {
        free(ptr);
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}
