//
// Copyright (c) 2015 
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
// 

//
// This file is part of tsc.
// 
// Tsc is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Tsc is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with tsc. If not, see <http://www.gnu.org/licenses/>
//

#ifndef TSC_SAMPARSER_H
#define TSC_SAMPARSER_H

#include "samrec.h"
#include "./str/str.h"
#include <stdbool.h>

typedef struct samparser_t_ {
    FILE     *fp;   // file pointer
    str_t    *head; // SAM header
    samrec_t curr;  // current SAM record
} samparser_t;

samparser_t * samparser_new(FILE *fp);
void samparser_free(samparser_t *samparser);
bool samparser_next(samparser_t *samparser);

#endif // TSC_SAMPARSER_H

