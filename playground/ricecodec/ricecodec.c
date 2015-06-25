/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of ricecodec.                                            *
 ******************************************************************************/

#include "ricecodec.h"
#include "tntlib_common.h"
#include <string.h>

typedef struct ricecodec_t_ {
    unsigned char  bbuf;        /* bit buffer          */
    size_t         bbuf_filled; /* bit count           */
    size_t         in_idx;      /* current 'in' index  */
    size_t         out_idx;     /* current 'out' index */
} ricecodec_t;

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static int ricecodec_len(unsigned char x, int k)
{
    int m = 1 << k;
    int q = x / m;
    return (q + 1 + k);
}

static void ricecodec_put_bit(ricecodec_t*   ricecodec,
                              unsigned char* out,
                              unsigned char  b)
{
    ricecodec->bbuf = ricecodec->bbuf | ((b & 1) << ricecodec->bbuf_filled);

    if (ricecodec->bbuf_filled == 7) {
        out[ricecodec->out_idx++] = ricecodec->bbuf;
        ricecodec->bbuf = 0x00;
        ricecodec->bbuf_filled = 0;
    } else {
        ricecodec->bbuf_filled++;
    }
}

static void ricecodec_encode(ricecodec_t*   ricecodec,
                             unsigned char* out,
                             unsigned char  x,
                             int            k)
{
    int m = 1 << k;
    int q = x / m;
    int i = 0;

    for (i = 0; i < q; i++)
        ricecodec_put_bit(ricecodec, out, 1);
    ricecodec_put_bit(ricecodec, out, 0);
    for (i = (k - 1); i >= 0; i--)
        ricecodec_put_bit(ricecodec, out, (x >> i) & 1);
}

static void ricecodec_init(ricecodec_t* ricecodec)
{
    ricecodec->bbuf = 0x00;
    ricecodec->bbuf_filled = 0;
    ricecodec->in_idx = 0;
    ricecodec->out_idx = 0;
}

unsigned char* ricecodec_compress(unsigned char* in,
                                  unsigned int   in_size,
                                  unsigned int*  out_size)
{
    ricecodec_t ricecodec;
    ricecodec_init(&ricecodec);

    /* Find best Rice parameter k. */
    unsigned int k = 0;
    unsigned int k_best = 0;
    size_t bit_cnt = 0;
    size_t bit_cnt_best = SIZE_MAX;
    for (k = 0; k < 8; k++) {
        size_t i = 0;
        for (i = 0; i < in_size; i++) bit_cnt += ricecodec_len(in[i], k);
        if (bit_cnt < bit_cnt_best) {
            bit_cnt_best = bit_cnt;
            k_best = k;
        }
    }

    /* Compute number of bytes needed. */
    *out_size = bit_cnt_best / 8;
    unsigned int bit_rem = (bit_cnt_best % 8) + 3; /* 3 bits for k */
    *out_size += (bit_rem / 8) + 1;

    /* Allocate enough memory for 'out'. */
    unsigned char* out = (unsigned char*)tnt_malloc(*out_size);
    memset(out, 0x00, *out_size);

    /* Output k. */
    ricecodec_put_bit(&ricecodec, out, (k_best >> 2) & 1);
    ricecodec_put_bit(&ricecodec, out, (k_best >> 1) & 1);
    ricecodec_put_bit(&ricecodec, out, (k_best     ) & 1);

    /* Encode. */
    size_t i = 0;
    for (i = 0; i < in_size; i++)
        ricecodec_encode(&ricecodec, out, in[i], k_best);

    /* Flush bit buffer. */
    size_t f = 8 - ricecodec.bbuf_filled;
    while (f--) ricecodec_put_bit(&ricecodec, out, 1);

    return out;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static unsigned char ricecodec_get_bit(ricecodec_t*   ricecodec,
                                       unsigned char* in)
{
    if (!(ricecodec->bbuf_filled)) {
        ricecodec->bbuf = in[ricecodec->in_idx++];
        ricecodec->bbuf_filled = 8;
    }

    unsigned char tmp = ricecodec->bbuf & 1;
    ricecodec->bbuf = ricecodec->bbuf >> 1;
    ricecodec->bbuf_filled--;

    return tmp;
}

unsigned char* ricecodec_decompress(unsigned char* in,
                                    unsigned int   in_size,
                                    unsigned int*  out_size)
{
    ricecodec_t ricecodec;
    ricecodec_init(&ricecodec);
    *out_size = 0;

    /* Allocate enough memory (bad estimate) for 'out'. */
    size_t out_alloc = 100 * in_size;
    unsigned char* out = (unsigned char*)tnt_malloc(out_alloc);

    unsigned int k = (ricecodec_get_bit(&ricecodec, in) << 2) |
                     (ricecodec_get_bit(&ricecodec, in) << 1) |
                     (ricecodec_get_bit(&ricecodec, in)     );

    do {
        int m = 1 << k, q = 0, x = 0, i = 0;

        while (ricecodec_get_bit(&ricecodec, in))
            q++;

        x = m * q;

        for (i = (k - 1); i >= 0; i--)
            x = x | (ricecodec_get_bit(&ricecodec, in) << i);

        out[ricecodec.out_idx++] = x;
        (*out_size)++;

        /* Allocate additional byte if needed. */
        if (ricecodec.out_idx == out_alloc) {
            out_alloc = *out_size + 1;
            out = (unsigned char*)tnt_realloc(out, out_alloc);
        }
    } while (ricecodec.in_idx < in_size);

    out = (unsigned char*)tnt_realloc(out, *out_size);

    return out;
}

