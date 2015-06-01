/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "frw.h"
#include "tsclib.h"
#include <string.h>

void fwrite_byte(FILE* fp, unsigned char byte)
{
    if (fwrite(&byte, 1, 1, fp) != 1)
        tsc_error("Could not write byte. Aborting.\n");
}

void fwrite_buf(FILE* fp, const unsigned char* buf, const size_t n)
{
    if (fwrite(buf, 1, n, fp) != n)
        tsc_error("Could not write %d bytes. Aborting.\n", n);
}

void fwrite_uint32(FILE* fp, uint32_t dword)
{
    fwrite_byte(fp, (dword >> 24) & 0xFF);
    fwrite_byte(fp, (dword >> 16) & 0xFF);
    fwrite_byte(fp, (dword >>  8) & 0xFF);
    fwrite_byte(fp, (dword      ) & 0xFF);
}

void fwrite_uint64(FILE* fp, uint64_t qword)
{
    fwrite_byte(fp, (qword >> 56) & 0xFF);
    fwrite_byte(fp, (qword >> 48) & 0xFF);
    fwrite_byte(fp, (qword >> 40) & 0xFF);
    fwrite_byte(fp, (qword >> 32) & 0xFF);
    fwrite_byte(fp, (qword >> 24) & 0xFF);
    fwrite_byte(fp, (qword >> 16) & 0xFF);
    fwrite_byte(fp, (qword >>  8) & 0xFF);
    fwrite_byte(fp, (qword      ) & 0xFF);
}

void fwrite_cstr(FILE* fp, const char* cstr)
{
    size_t nbytes = strlen(cstr);
    unsigned int i = 0;
    for (i = 0; i < nbytes; i++) {
        fwrite_byte(fp, cstr[i]);
    }
}

size_t fread_byte(FILE* fp, unsigned char* byte)
{
    return fread(byte, 1, 1, fp);
}

size_t fread_buf(FILE* fp, unsigned char* buf, const size_t n)
{
    return fread(buf, 1, n, fp);
}

size_t fread_uint32(FILE* fp, uint32_t* dword)
{
    unsigned char* bytes = (unsigned char*)malloc(4);
    if (fread(bytes, 1, 4, fp) != 4) {
        free((void*)bytes);
        tsc_error("Could not read 4 bytes. Aborting.\n");
    }
    *dword = (uint32_t)bytes[0] << 24 |
             (uint32_t)bytes[1] << 16 |
             (uint32_t)bytes[2] <<  8 |
             (uint32_t)bytes[3];
    free((void*)bytes);

    return sizeof(uint32_t);
}

size_t fread_uint64(FILE* fp, uint64_t* qword)
{
    unsigned char* bytes = (unsigned char*)malloc(8);
    if (fread(bytes, 1, 8, fp) != 8) {
        free((void*)bytes);
        tsc_error("Could not read 8 bytes. Aborting.\n");
    }
    *qword = (uint64_t)bytes[0] << 56 |
             (uint64_t)bytes[1] << 48 |
             (uint64_t)bytes[2] << 40 |
             (uint64_t)bytes[3] << 32 |
             (uint64_t)bytes[4] << 24 |
             (uint64_t)bytes[5] << 16 |
             (uint64_t)bytes[6] <<  8 |
             (uint64_t)bytes[7];
    free((void*)bytes);

    return sizeof(uint64_t);
}

