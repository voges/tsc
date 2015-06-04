/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_FRW_H
#define TSC_FRW_H

#include <stdint.h>
#include <stdio.h>

void fwrite_byte(FILE* fp, unsigned char byte);
void fwrite_buf(FILE* fp, const unsigned char* buf, const size_t n);
void fwrite_uint32(FILE* fp, uint32_t dword);
void fwrite_uint64(FILE* fp, uint64_t qword);
void fwrite_cstr(FILE* fp, const char* cstr);
size_t fread_byte(FILE* fp, unsigned char* byte);
size_t fread_buf(FILE* fp, unsigned char* buf, const size_t n);
size_t fread_uint32(FILE* fp, uint32_t* dword);
size_t fread_uint64(FILE* fp, uint64_t* qword);

#endif /* TSC_FRW_H */

