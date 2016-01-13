/*
 * The copyright in this software is being made available under the TNT
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2015, Leibniz Universitaet Hannover, Institut fuer
 * Informationsverarbeitung (TNT)
 * Contact: <voges@tnt.uni-hannover.de>
 * All rights reserved.
 *
 * * Redistribution in source or binary form is not permitted.
 *
 * * Use in source or binary form is only permitted in the context of scientific
 *   research.
 *
 * * Commercial use without specific prior written permission is prohibited.
 *   Neither the name of the TNT nor the names of its contributors may be used
 *   to endorse or promote products derived from this software without specific
 *   prior written permission.
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

#include "common/str.h"
#include <stdint.h>
#include <stdio.h>

typedef struct auxcodec_t_ {
    size_t        record_cnt; // No. of records processed in the current block
    str_t         *uncompressed;
    unsigned char *compressed;
    size_t        compressed_sz;
} auxcodec_t;

auxcodec_t * auxcodec_new(void);
void auxcodec_free(auxcodec_t *auxcodec);

// Encoder methods
// -----------------------------------------------------------------------------

void auxcodec_add_record(auxcodec_t     *auxcodec,
                         const uint16_t flag,
                         const uint8_t  mapq,
                         const char     *opt);
size_t auxcodec_write_block(auxcodec_t *auxcodec, FILE *fp);

// Decoder methods
// -----------------------------------------------------------------------------

size_t auxcodec_decode_block(auxcodec_t *auxcodec,
                             FILE       *fp,
                             uint16_t   *flag,
                             uint8_t    *mapq,
                             str_t      **opt);

#endif // TSC_AUXCODEC_H

