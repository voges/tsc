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

#ifndef TSC_SAMCODEC_H
#define TSC_SAMCODEC_H

#include "samparser.h"
#include "codecs/auxcodec.h"
#include "codecs/idcodec.h"
//#include "codecs/nuccodec_o0.h"
#include "codecs/nuccodec_o1.h"
#include "codecs/paircodec.h"
#include "codecs/qualcodec.h"
#include "tvclib/str.h"
#include <stdio.h>

typedef struct samenc_t_ {
    FILE         *ifp;
    FILE         *ofp;
    unsigned int blk_sz;
    samparser_t  *samparser;
    auxenc_t     *auxenc;
    idenc_t      *idenc;
    nucenc_t     *nucenc;
    pairenc_t    *pairenc;
    qualenc_t    *qualenc;
} samenc_t;

samenc_t * samenc_new(FILE *ifp, FILE *ofp, unsigned int blk_sz);
void samenc_free(samenc_t *samenc);
void samenc_encode(samenc_t *samenc);

typedef struct samdec_t_ {
    FILE      *ifp;
    FILE      *ofp;
    auxdec_t  *auxdec;
    iddec_t   *iddec;
    nucdec_t  *nucdec;
    pairdec_t *pairdec;
    qualdec_t *qualdec;
} samdec_t;

samdec_t * samdec_new(FILE *ifp, FILE *ofp);
void samdec_free(samdec_t *samdec);
void samdec_decode(samdec_t *samdec);
void samdec_info(samdec_t *samdec);

#endif // TSC_SAMCODEC_H

