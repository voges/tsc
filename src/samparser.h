/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#ifndef TSC_SAMPARSER_H
#define TSC_SAMPARSER_H

#include "samrecord.h"
#include "str.h"
#include <stdbool.h>

typedef struct samparser_t_ {
    FILE*       fp;   /* file pointer       */
    str_t*      head; /* SAM header         */
    samrecord_t curr; /* current SAM record */
} samparser_t;

samparser_t* samparser_new(FILE* fp);
void samparser_free(samparser_t* samparser);
bool samparser_next(samparser_t* samparser);

#endif /* TSC_SAMPARSER_H */

