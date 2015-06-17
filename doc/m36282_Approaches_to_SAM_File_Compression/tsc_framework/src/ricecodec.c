/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "ricecodec.h"
#include "tsclib.h"
#include <math.h>

unsigned char rc_bbuf = 0;
int rc_bbuf_filled = 0;
unsigned int rc_rc_out_idx = 0;
unsigned int rc_in_idx = 0;
unsigned char* rc_out;

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static int ricecodec_len(unsigned char x, int k)
{
    int m = 1 << k;
    int q = x / m;
    return (q + 1 + k);
}

static void ricecodec_put_bit(unsigned char* rc_out, unsigned char  b)
{
    rc_bbuf = rc_bbuf | ((b & 1) << rc_bbuf_filled);

    if (rc_bbuf_filled == 7) {
        rc_out[rc_rc_out_idx++] = rc_bbuf;
        rc_bbuf = 0;
        rc_bbuf_filled = 0;
    } else {
        rc_bbuf_filled++;
    }
}

void ricecodec_encode(unsigned char x, int k)
{
    int m = 1 << k;
    int q = x / m;
    int i = 0;

    for (i = 0; i < q; i++) ricecodec_put_bit(rc_out, 1);
    ricecodec_put_bit(rc_out, 0);

    for (i = (k - 1); i >= 0; i--) ricecodec_put_bit(rc_out, (x >> i) & 1);
}

unsigned char *ricecodec_compress(unsigned char* in,
                                  unsigned int   in_size,
                                  unsigned int*  rc_out_size)
{
    rc_bbuf = 0;
    rc_bbuf_filled = 0;
    rc_rc_out_idx = 0;
    rc_in_idx = 0;

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
    *rc_out_size = ceil(bit_cnt_best / 8);

    /* Allocate enough memory for 'rc_out'. */
    rc_out = (unsigned char*)tsc_malloc(*rc_out_size);

    /* rc_output. */
    ricecodec_put_bit(rc_out, (k_best >> 2) & 1);
    ricecodec_put_bit(rc_out, (k_best >> 1) & 1);
    ricecodec_put_bit(rc_out, (k_best     ) & 1);

    size_t i = 0;
    for (i = 0; i < in_size; i++)
        ricecodec_encode(in[i], k_best);

    /* Flush bit buffer. */
    i = 8 - rc_bbuf_filled;
    while (i--) ricecodec_put_bit(rc_out, 1);

    return rc_out;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
unsigned char ricecodec_get_bit(unsigned char* in)
{
    if (!(rc_bbuf_filled)) {
        rc_bbuf = in[rc_in_idx++];
        rc_bbuf_filled = 8;
    }

    unsigned char tmp = rc_bbuf & 1;
    rc_bbuf = rc_bbuf >> 1;
    rc_bbuf_filled--;

    return tmp;
}

unsigned char *ricecodec_uncompress(unsigned char* in,
                                    unsigned int   in_size,
                                    unsigned int*  rc_out_size)
{
    rc_bbuf = 0;
    rc_bbuf_filled = 0;
    rc_rc_out_idx = 0;
    rc_in_idx = 0;

    *rc_out_size = 0;

    /* Allocate enough memory for 'rc_out'. */
    rc_out = (unsigned char*)tsc_malloc(100 * in_size);

    unsigned int k = (ricecodec_get_bit(in) << 2) |
                     (ricecodec_get_bit(in) << 1) |
                     (ricecodec_get_bit(in)     );

    while (1) {
        int m = 1 << k, q = 0, x, i;

        while (ricecodec_get_bit(in)) q++;
        x = m * q;

        for (i = (k - 1); i >= 0; i--)
            x = x | (ricecodec_get_bit(in) << i);

        if (rc_in_idx == in_size) break;

        rc_out[rc_rc_out_idx++] = x;
        (*rc_out_size)++;
    }

    rc_out = (unsigned char*)tsc_realloc(rc_out, *rc_out_size);

    return rc_out;
}

