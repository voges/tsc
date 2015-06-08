#include <stdio.h>
#include "ac_test.h"
#include <stdint.h>
#include <stdlib.h>

#define BLK_SIZE 1000000
// Room to allow for expanded BLK_SIZE on worst case compression.
#define BLK_SIZE2 ((int)(1.05*BLK_SIZE+10))

int main(int argc, char **argv) {
    int opt;
    unsigned char in_buf[(int)(BLK_SIZE2+257*257*3 + 37)];
    int decode = 0;
    FILE *infp = stdin, *outfp = stdout;
    size_t bytes = 0;

    extern char *optarg;
    extern int optind, opterr, optopt;

    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {

        case 'd':
            decode = 1;
            break;
        }
    }


    if (optind < argc) {
        if (!(infp = fopen(argv[optind], "rb"))) {
            perror(argv[optind]);
            return 1;
        }
        optind++;
    }

    if (optind < argc) {
        if (!(outfp = fopen(argv[optind], "wb"))) {
            perror(argv[optind]);
            return 1;
        }
        optind++;
    }


    if (decode) {
        // Only used in some test implementations of RC_GetFreq()
        RC_init();
        RC_init2();

        for (;;) {
            uint32_t in_size, out_size;
            unsigned char *out;

            if (4 != fread(&in_size, 1, 4, infp))
                break;
            if (in_size != fread(in_buf, 1, in_size, infp)) {
                fprintf(stderr, "Truncated input\n");
                exit(1);
            }
            out = arith_uncompress_O0(in_buf, in_size, &out_size);
            if (!out)
                abort();

            fwrite(out, 1, out_size, outfp);
            free(out);

            bytes += out_size;
        }
    } else {
        for (;;) {
            uint32_t in_size, out_size;
            unsigned char *out;

            in_size = fread(in_buf, 1, BLK_SIZE, infp);
            if (in_size <= 0)
                break;

            out = arith_compress_O0(in_buf, in_size, &out_size);

            fwrite(&out_size, 1, 4, outfp);
            fwrite(out, 1, out_size, outfp);
            free(out);

            bytes += in_size;
        }
    }

    return 0;
}
