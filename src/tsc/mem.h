// Copyright 2015 Leibniz University Hannover (LUH)

#ifndef TSC_MEM_H_
#define TSC_MEM_H_

#include <stdlib.h>

void *tsc_malloc(size_t size);

void *tsc_realloc(void *ptr, size_t size);

void tsc_free(void *ptr);

#endif  // TSC_MEM_H_
