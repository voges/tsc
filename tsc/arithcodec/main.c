/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of arithcodec.                                           *
 ******************************************************************************/

#include "arithcodec.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#define BLK_SIZE 1000000
/* Room to allow for expanded BLK_SIZE on worst case compression. */
#define BLK_SIZE2 ((int)(1.05*BLK_SIZE+10))

int main(int argc, char **argv) {
    int opt;
    unsigned char in_buf[(int)(BLK_SIZE2+257*257*3 + 37)];
    bool decode = false;
    FILE* in_fp = stdin;
    FILE* out_fp = stdout;
    size_t bytes = 0;

    extern char *optarg;
    extern int optind, opterr, optopt;

    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
        case 'd':
            decode = true;
            break;
        }
    }

    if (optind < argc) {
        if (!(in_fp = fopen(argv[optind], "rb"))) {
            perror(argv[optind]);
            return EXIT_FAILURE;
        }
        optind++;
    }

    if (optind < argc) {
        if (!(out_fp = fopen(argv[optind], "wb"))) {
            perror(argv[optind]);
            return EXIT_FAILURE;
        }
        optind++;
    }

    unsigned int in_size = 0, out_size = 0;
    unsigned char* out;

    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);

    if (decode) {
        for (;;) {
            if (4 != fread(&in_size, 1, 4, in_fp)) break;
            if (in_size != fread(in_buf, 1, in_size, in_fp)) {
                fprintf(stderr, "Truncated input\n");
                exit(EXIT_FAILURE);
            }
            out = arith_decompress_o0(in_buf, in_size, &out_size);
            if (!out) abort();
            fwrite(out, 1, out_size, out_fp);
            free(out);
            bytes += out_size;
        }
    } else {
        for (;;) {
            in_size = fread(in_buf, 1, BLK_SIZE, in_fp);
            if (in_size <= 0) break;
            out = arith_compress_o0(in_buf, in_size, &out_size);
            fwrite(&out_size, 1, 4, out_fp);
            fwrite(out, 1, out_size, out_fp);
            free((void*)out);
            bytes += in_size;
        }
    }

    gettimeofday(&tv2, NULL);

    fprintf(stderr, "Took %ld microseconds, %.1f MB/s\n",
            (long)(tv2.tv_sec - tv1.tv_sec)*1000000 +
            tv2.tv_usec - tv1.tv_usec,
            (double)bytes / ((long)(tv2.tv_sec - tv1.tv_sec)*1000000 +
            tv2.tv_usec - tv1.tv_usec));

    return EXIT_SUCCESS;
}

