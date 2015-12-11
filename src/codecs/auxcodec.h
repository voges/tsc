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

#ifndef TSC_AUXCODEC_H
#define TSC_AUXCODEC_H

#include "../tvclib/str.h"
#include <stdint.h>
#include <stdio.h>

// Encoder
// -----------------------------------------------------------------------------

typedef struct auxenc_t_ {
    size_t in_sz;   // Accumulated input size
    size_t rec_cnt; // No. of records processed in the current block
    str_t  *tmp;    // Temporal storage for e.g. prediction residues
} auxenc_t;

auxenc_t * auxenc_new(void);
void auxenc_free(auxenc_t *auxenc);
void auxenc_add_record(auxenc_t       *auxenc,
                       const uint16_t flag,
                       const uint8_t  mapq,
                       const char     *opt);
size_t auxenc_write_block(auxenc_t *auxenc, FILE *fp);

// Decoder
// -----------------------------------------------------------------------------

typedef struct auxdec_t_ {
    size_t out_sz; // Accumulated output size
} auxdec_t;

auxdec_t * auxdec_new(void);
void auxdec_free(auxdec_t *auxdec);
size_t auxdec_decode_block(auxdec_t *auxdec,
                           FILE     *fp,
                           uint16_t *flag,
                           uint8_t  *mapq,
                           str_t    **opt);

#endif // TSC_AUXCODEC_H

