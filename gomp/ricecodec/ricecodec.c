/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <voges@tnt.uni-hannover.de>
 */

/*
 * Note it is up to the calling code to ensure that no overruns on input and
 * output buffers occur.
 */

#include "ricecodec.h"
#include <stdbool.h>
#include <string.h>

#define KB 1024LL
#define MB (KB * 1024LL)

#ifndef SIZE_MAX
#define SIZE_MAX (size_t)-1 /* due to portability */
#endif

typedef struct ricecodec_t_ {
    unsigned char  bbuf;        /* bit buffer           */
    size_t         bbuf_filled; /* bit count            */
    size_t         in_idx;      /* current 'in' index   */
    size_t         out_idx;     /* current 'out' index  */
    size_t         in_size;     /* 'in' size            */
    bool           eof;         /* flag to indicate EOF */
} ricecodec_t;

static void ricecodec_init(ricecodec_t* rc, size_t in_size)
{
    rc->bbuf = 0x00;
    rc->bbuf_filled = 0;
    rc->in_idx = 0;
    rc->out_idx = 0;
    rc->in_size = in_size;
    rc->eof = false;
}

/*
 * Encoder
 * -----------------------------------------------------------------------------
 */

static int codelen(unsigned char x, int k)
{
    int m = 1 << k;
    int q = x / m;
    return (q + 1 + k);
}

static void put_bit(ricecodec_t* rc, unsigned char* out, unsigned char b)
{
    rc->bbuf = rc->bbuf | ((b & 1) << rc->bbuf_filled);

    if (rc->bbuf_filled == 7) {
        out[rc->out_idx++] = rc->bbuf;
        rc->bbuf = 0x00;
        rc->bbuf_filled = 0;
    } else {
        rc->bbuf_filled++;
    }
}

static void encode(ricecodec_t*   rc,
                   unsigned char* out,
                   unsigned char  x,
                   int            k)
{
    int m = 1 << k;
    int q = x / m;
    int i = 0;

    for (i = 0; i < q; i++)
        put_bit(rc, out, 1);

    put_bit(rc, out, 0);

    for (i = (k - 1); i >= 0; i--)
        put_bit(rc, out, (x >> i) & 1);
}

unsigned char* rice_compress(unsigned char* in,
                             size_t         in_size,
                             size_t*        out_size)
{
    ricecodec_t rc;
    ricecodec_init(&rc, in_size);

    /* Find best Rice parameter k. */
    unsigned int k = 0;
    unsigned int k_best = 0;
    size_t bit_cnt = 0;
    size_t bit_cnt_best = SIZE_MAX;
    for (k = 0; k < 8; k++) {
        size_t i = 0;
        for (i = 0; i < in_size; i++) bit_cnt += codelen(in[i], k);
        if (bit_cnt < bit_cnt_best) {
            bit_cnt_best = bit_cnt;
            k_best = k;
        }
    }

    /* Compute number of bytes needed. */
    *out_size = bit_cnt_best / 8;
    unsigned int bit_rem = (bit_cnt_best % 8) + 3; /* +3 bits for k e [0-7] */
    *out_size += (bit_rem / 8) + 1;

    /* Allocate enough memory for 'out'. */
    unsigned char* out = (unsigned char*)malloc(*out_size);
    if (!out) abort();
    memset(out, 0x00, *out_size);

    /* Output k. */
    put_bit(&rc, out, (k_best >> 2) & 1);
    put_bit(&rc, out, (k_best >> 1) & 1);
    put_bit(&rc, out, (k_best     ) & 1);

    /* Encode. */
    size_t i = 0;
    for (i = 0; i < in_size; i++)
        encode(&rc, out, in[i], k_best);

    /* Flush bit buffer: Fill with 1s, so decoder will run into EOF. */
    size_t f = 8 - rc.bbuf_filled;
    while (f--)
        put_bit(&rc, out, 1);

    return out;
}

/*
 * Decoder
 * -----------------------------------------------------------------------------
 */

static int get_bit(ricecodec_t* rc, unsigned char* in)
{
    if (!(rc->bbuf_filled)) {
        if (rc->in_idx < rc->in_size) {
            rc->bbuf = in[rc->in_idx++];
            rc->bbuf_filled = 8;
        } else {
            rc->eof = true; /* reached EOF */
        }
    }

    int tmp = rc->bbuf & 0x01;
    rc->bbuf = rc->bbuf >> 1;
    rc->bbuf_filled--;

    return tmp;
}

unsigned char* rice_decompress(unsigned char* in,
                               size_t         in_size,
                               size_t*        out_size)
{
    ricecodec_t rc;
    ricecodec_init(&rc, in_size);
    *out_size = 0;

    /* Allocate enough memory (100 is a bad estimate) for 'out'. */
    size_t out_alloc = 100 * in_size;
    unsigned char* out = (unsigned char*)malloc(out_alloc);
    if (!out) abort();

    unsigned int k = (get_bit(&rc, in) << 2) |
                     (get_bit(&rc, in) << 1) |
                     (get_bit(&rc, in)     );

    while (1) {
        int m = 1 << k, q = 0, x = 0, i = 0;

        while (get_bit(&rc, in))
            q++;

        if (rc.eof) break;

        x = m * q;

        for (i = (k - 1); i >= 0; i--)
            x = x | (get_bit(&rc, in) << i);

        out[rc.out_idx++] = x;
        (*out_size)++;

        /* Allocate additional page if needed. */
        if (rc.out_idx == out_alloc) {
            out_alloc = *out_size + (4 * KB);
            out = (unsigned char*)realloc(out, out_alloc);
            if (!out) abort();
        }
    }

    out = (unsigned char*)realloc(out, *out_size);
    if (!out) abort();

    return out;
}

#if TEST_MAIN

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

int main(int argc, char* argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Exactly two arguments needed: rice encode/decode "
                "<infile> <outfile>\n");
        exit(EXIT_FAILURE);
    }

    bool encode = false;
    if (!strcmp(argv[1], "encode")) {
        encode = true;
    } else if (!strcmp(argv[1], "decode")) {
        encode = false;
    } else {
        fprintf(stderr, "Syntax: rice encode/decode <file>\n");
        exit(EXIT_FAILURE);
    }

    const char* ifn = argv[2];
    FILE* ifp = fopen(ifn, "r");
    if (!ifp) {
        fprintf(stderr, "Error: Could not open file: %s\n", ifn);
        exit(EXIT_FAILURE);
    }

    /* Get infile size. */
    fseek(ifp, 0L, SEEK_END);
    size_t in_sz = ftell(ifp);
    fprintf(stderr, "%s: %zu bytes\n", ifn, in_sz);
    rewind(ifp);

    /* Read file content into buffer. */
    unsigned char* in = (unsigned char*)malloc(in_sz);
    fread(in, 1, in_sz, ifp);
    fclose(ifp);

    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);

    /* Compress or decompress. */
    unsigned char* out;
    size_t out_sz = 0;
    if (encode) out = rice_compress(in, in_sz, &out_sz);
    else out = rice_decompress(in, in_sz, &out_sz);
    if (!out) abort();

    /* Write to outfile. */
    const char* ofn = argv[3];
    FILE* ofp = fopen(ofn, "w");
    if (!ofp) {
        fprintf(stderr, "Error: Could not open file: %s\n", ofn);
        exit(EXIT_FAILURE);
    }
    fwrite(out, 1, out_sz, ofp);
    fclose(ofp);

    gettimeofday(&tv2, NULL);

    /* Write (de-)compression ratio and elapsed time. */
    fprintf(stderr, "%s: %zu bytes (%.2f%%)\n", ofn, out_sz,
            (double)(((double)out_sz / (double)in_sz) * 100));

    fprintf(stderr, "Took %ld microseconds, %.1f MB/s\n",
            (long)(tv2.tv_sec - tv1.tv_sec)*1000000 +
            tv2.tv_usec - tv1.tv_usec,
            (double)out_sz / ((long)(tv2.tv_sec - tv1.tv_sec)*1000000 +
            tv2.tv_usec - tv1.tv_usec));

    free(out);
    free(in);

    return EXIT_SUCCESS;
}

#endif /* TEST_MAIN */

