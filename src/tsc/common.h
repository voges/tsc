// Copyright 2015 Leibniz University Hannover (LUH)

#ifndef TSC_COMMON_H_
#define TSC_COMMON_H_

#include <stdbool.h>
#include <stdlib.h>

bool yesno();

long tv_diff(struct timeval tv0, struct timeval tv1);

size_t num_digits(int64_t x);

#endif  // TSC_COMMON_H_
