/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

/*
 * Wrapper functions to safely read/write different data types from/to files (or
 * streams).
 * The 'fwrite_uintXX' and 'fread_uintXX' functions are always compatible with
 * each other and are independent from the endianness of the system this file
 * will be built on.
 */

#ifndef TSC_FRW_H
#define TSC_FRW_H

#include <stdint.h>
#include <stdio.h>

size_t fwrite_byte(FILE* fp, const unsigned char byte);
size_t fwrite_buf(FILE* fp, const unsigned char* buf, const size_t n);
size_t fwrite_uint32(FILE* fp, const uint32_t dword);
size_t fwrite_uint64(FILE* fp, const uint64_t qword);
size_t fwrite_cstr(FILE* fp, const char* cstr);
size_t fread_byte(FILE* fp, unsigned char* byte);
size_t fread_buf(FILE* fp, unsigned char* buf, const size_t n);
size_t fread_uint32(FILE* fp, uint32_t* dword);
size_t fread_uint64(FILE* fp, uint64_t* qword);

#endif /* TSC_FRW_H */

