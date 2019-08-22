#ifndef TSC_COMMON_H
#define TSC_COMMON_H

#include <stdbool.h>
#include <stdlib.h>

bool yesno(void);
long tvdiff(struct timeval tv0, struct timeval tv1);
size_t ndigits(int64_t x);

#endif  // TSC_COMMON_H
