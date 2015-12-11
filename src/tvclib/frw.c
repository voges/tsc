/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2015, Leibniz Universitaet Hannover, Institut fuer
 * Informationsverarbeitung (TNT)
 * Contact: <voges@tnt.uni-hannover.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the TNT nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "frw.h"
#include <stdlib.h>

size_t fwrite_byte(FILE *fp, const unsigned char byte)
{
    if (fwrite(&byte, 1, 1, fp) != 1) {
        fprintf(stderr, "Error: Could not write byte\n");
        exit(EXIT_FAILURE);
    }
    return 1;
}

size_t fwrite_buf(FILE *fp, const unsigned char *buf, const size_t n)
{
    if (fwrite(buf, 1, n, fp) != n) {
        fprintf(stderr, "Error: Could not write %zu byte(s)\n", n);
        exit(EXIT_FAILURE);
    }
    return n;
}

size_t fwrite_uint32(FILE *fp, const uint32_t dword)
{
    fwrite_byte(fp, (unsigned char)(dword >> 24) & 0xFF);
    fwrite_byte(fp, (unsigned char)(dword >> 16) & 0xFF);
    fwrite_byte(fp, (unsigned char)(dword >>  8) & 0xFF);
    fwrite_byte(fp, (unsigned char)(dword      ) & 0xFF);
    return sizeof(uint32_t);
}

size_t fwrite_uint64(FILE *fp, const uint64_t qword)
{
    fwrite_byte(fp, (unsigned char)(qword >> 56) & 0xFF);
    fwrite_byte(fp, (unsigned char)(qword >> 48) & 0xFF);
    fwrite_byte(fp, (unsigned char)(qword >> 40) & 0xFF);
    fwrite_byte(fp, (unsigned char)(qword >> 32) & 0xFF);
    fwrite_byte(fp, (unsigned char)(qword >> 24) & 0xFF);
    fwrite_byte(fp, (unsigned char)(qword >> 16) & 0xFF);
    fwrite_byte(fp, (unsigned char)(qword >>  8) & 0xFF);
    fwrite_byte(fp, (unsigned char)(qword      ) & 0xFF);
    return sizeof(uint64_t);
}

size_t fread_byte(FILE *fp, unsigned char *byte)
{
    return fread(byte, 1, 1, fp);
}

size_t fread_buf(FILE *fp, unsigned char *buf, const size_t n)
{
    return fread(buf, 1, n, fp);
}

size_t fread_uint32(FILE *fp, uint32_t *dword)
{
    unsigned char *bytes = (unsigned char *)malloc(sizeof(uint32_t));
    if (!bytes) abort();
    size_t ret = fread(bytes, 1, sizeof(uint32_t), fp);

    if (ret != sizeof(uint32_t)) {
        free(bytes);
        return ret;
    }

    *dword = (uint32_t)bytes[0] << 24 |
             (uint32_t)bytes[1] << 16 |
             (uint32_t)bytes[2] <<  8 |
             (uint32_t)bytes[3];

    free(bytes);
    return ret;
}

size_t fread_uint64(FILE *fp, uint64_t *qword)
{
    unsigned char *bytes = (unsigned char *)malloc(sizeof(uint64_t));
    if (!bytes) abort();
    size_t ret = fread(bytes, 1, sizeof(uint64_t), fp);

    if (ret != sizeof(uint64_t)) {
        free(bytes);
        return ret;
    }

    *qword = (uint64_t)bytes[0] << 56 |
             (uint64_t)bytes[1] << 48 |
             (uint64_t)bytes[2] << 40 |
             (uint64_t)bytes[3] << 32 |
             (uint64_t)bytes[4] << 24 |
             (uint64_t)bytes[5] << 16 |
             (uint64_t)bytes[6] <<  8 |
             (uint64_t)bytes[7];

    free(bytes);
    return ret;
}

