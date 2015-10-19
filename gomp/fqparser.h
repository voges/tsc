/*
 * Copyright (c) 2015 
 * Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <voges@tnt.uni-hannover.de>
 */

/*
 * This file is part of gomp.
 *
 * Gomp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gomp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gomp. If not, see <http://www.gnu.org/licenses/>
 */

#ifndef GOMP_FQPARSER_H
#define GOMP_FQPARSER_H

#include "fqrecord.h"
#include <stdbool.h>

typedef struct fqparser_t_ {
    FILE*   fp;   /* file pointer         */
    fqrec_t curr; /* current FASTQ record */
} fqparser_t;

fqparser_t* fqparser_new(FILE* fp);
void fqparser_free(fqparser_t* fqparser);
bool fqparser_next(fqparser_t* fqparser);

#endif /* GOMP_FQPARSER_H */

