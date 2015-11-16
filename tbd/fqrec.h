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

#ifndef GOMP_FQREC_H
#define GOMP_FQREC_H

typedef struct fqrec_t_ {
    char rhead[8*MB];
    char seq[8*MB];
    char desc[8*MB];
    char qual[8*MB];
} fqrec_t;

#endif /* GOMP_FQREC_H */

