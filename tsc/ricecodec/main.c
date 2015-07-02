/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of ricecodec.                                            *
 ******************************************************************************/

#include "ricecodec.h"
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
    }
    else if (!strcmp(argv[1], "decode")) {
        encode = false;
    }
    else {
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

    free((void*)out);
    free((void*)in);

    return EXIT_SUCCESS;
}

