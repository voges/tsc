// Copyright 2015 Leibniz University Hannover (LUH)

#ifndef TSC_SAMPARSER_H_
#define TSC_SAMPARSER_H_

#include <stdbool.h>
#include <stdio.h>

#include "samrec.h"
#include "str.h"

typedef struct samparser_t_ {
    FILE *fp;
    str_t *head;
    samrec_t curr;
} samparser_t;

samparser_t *samparser_new(FILE *fp);

void samparser_free(samparser_t *samparser);

void samparser_head(samparser_t *samparser);

bool samparser_next(samparser_t *samparser);

#endif  // TSC_SAMPARSER_H_
