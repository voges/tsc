#include "mem.h"
#include <stdio.h>

void *tsc_malloc(const size_t size) {
    void *p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "tsc: error: Cannot allocate %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }
    return p;
}

void *tsc_realloc(void *ptr, const size_t size) {
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "tsc: error: Cannot allocate %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }
    return p;
}

// void tsc_free(void *ptr)
// {
//     if (ptr != NULL) {
//         free(ptr);
//         // ptr = NULL;
//     } else {
//         fprintf(stderr, "tsc: error: Tried to free null pointer\n");
//         exit(EXIT_FAILURE);
//     }
// }
