/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "ricecodec.h"
#include "tsclib.h"
#include <math.h>

unsigned char bbuf = 0;
int filled = 0;
unsigned int out_idx = 0;
unsigned int in_idx = 0;
unsigned char* out;

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static int ricecodec_len(unsigned char x, int k)
{
    int m = 1 << k;
    int q = x / m;
    return (q + 1 + k);
}

static void ricecodec_put_bit(unsigned char* out, unsigned char  b)
{
    bbuf = bbuf | ((b & 1) << filled);

    if (filled == 7) {
        out[out_idx++] = bbuf;
        bbuf = 0;
        filled = 0;
    } else {
        filled++;
    }
}

void ricecodec_encode(unsigned char x, int k)
{
    int m = 1 << k;
    int q = x / m;
    int i = 0;

    for (i = 0; i < q; i++) ricecodec_put_bit(out, 1);
    ricecodec_put_bit(out, 0);

    for (i = (k - 1); i >= 0; i--) ricecodec_put_bit(out, (x >> i) & 1);
}

unsigned char *ricecodec_compress(unsigned char* in,
                                  unsigned int   in_size,
                                  unsigned int*  out_size)
{
    bbuf = 0;
    filled = 0;
    out_idx = 0;
    in_idx = 0;

    /* Find best Rice parameter k. */
    unsigned int k = 0;
    unsigned int k_best = 0;
    unsigned int bit_cnt = 0;
    unsigned int bit_cnt_best = 0xFFFFFFFF;
    for (k = 0; k < 8; k++) {
        size_t i = 0;
        for (i = 0; i < in_size; i++) bit_cnt += ricecodec_len(in[i], k);
        if (bit_cnt < bit_cnt_best) {
            bit_cnt_best = bit_cnt;
            k_best = k;
        }
    }
    *out_size = ceil(bit_cnt_best / 8);

    /* Allocate enough memory for 'out'. */
    out = (unsigned char*)tsc_malloc(*out_size);

    /* Output. */
    ricecodec_put_bit(out, (k_best >> 2) & 1);
    ricecodec_put_bit(out, (k_best >> 1) & 1);
    ricecodec_put_bit(out, (k_best     ) & 1);

    size_t i = 0;
    for (i = 0; i < in_size; i++)
        ricecodec_encode(in[i], k_best);

    /* Flush bit buffer. */
    i = 8 - filled;
    while (i--) ricecodec_put_bit(out, 1);

    return out;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
unsigned char ricecodec_get_bit(unsigned char* in)
{
    if (!(filled)) {
        bbuf = in[in_idx++];
        filled = 8;
    }

    unsigned char tmp = bbuf & 1;
    bbuf = bbuf >> 1;
    filled--;

    return tmp;
}

unsigned char *ricecodec_uncompress(unsigned char* in,
                                    unsigned int   in_size,
                                    unsigned int*  out_size)
{
    bbuf = 0;
    filled = 0;
    out_idx = 0;
    in_idx = 0;

    *out_size = 0;

    /* Allocate enough memory for 'out'. */
    out = (unsigned char*)tsc_malloc(100 * in_size);

    unsigned int k = (ricecodec_get_bit(in) << 2) |
                     (ricecodec_get_bit(in) << 1) |
                     (ricecodec_get_bit(in)     );

    while (1) {
        int m = 1 << k, q = 0, x, i;

        while (ricecodec_get_bit(in)) q++;
        x = m * q;

        for (i = (k - 1); i >= 0; i--)
            x = x | (ricecodec_get_bit(in) << i);

        if (in_idx == in_size) break;

        out[out_idx++] = x;
        (*out_size)++;
    }

    out = (unsigned char*)tsc_realloc(out, *out_size);

    return out;
}

