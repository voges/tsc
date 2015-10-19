/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
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
 * along with gomp. If not, see <http: *www.gnu.org/licenses/>
 */

/*
 * Wrapper functions to safely read/write different data types.
 * The 'fwrite_uintXX' and 'fread_uintXX' functions are compatible with
 * each other and are independent from the endianness of the system this code
 * will be built on.
 */

#ifndef GOMP_FRW_H
#define GOMP_FRW_H

#include <stdint.h>
#include <stdio.h>

size_t fwrite_byte(FILE* fp, const unsigned char byte);
size_t fwrite_buf(FILE* fp, const unsigned char* buf, const size_t n);
size_t fwrite_uint32(FILE* fp, const uint32_t dword);
size_t fwrite_uint64(FILE* fp, const uint64_t qword);
size_t fread_byte(FILE* fp, unsigned char* byte);
size_t fread_buf(FILE* fp, unsigned char* buf, const size_t n);
size_t fread_uint32(FILE* fp, uint32_t* dword);
size_t fread_uint64(FILE* fp, uint64_t* qword);

#endif /* GOMP_FRW_H */

