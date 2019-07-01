#ifndef TSC_MEM_H
#define TSC_MEM_H

#include <stdlib.h>

void * tsc_malloc(size_t size);
void * tsc_realloc(void *ptr, size_t size);
// void tsc_free(void *ptr);

#endif // TSC_MEM_H
