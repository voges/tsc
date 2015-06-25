#include "ricecodec.h"
#include "tntlib_common.h"
#include <stdbool.h>
#include <string.h>

int main(int argc, char* argv[])
{
    if (argc != 4) tnt_error("Exactly two arguments needed: rice encode/decode "
                             "<infile> <outfile>\n");

    bool encode = false;
    if (!strcmp(argv[1], "encode"))
        encode = true;
    else if (!strcmp(argv[1], "decode"))
        encode = false;
    else
        tnt_error("Syntax: rice encode/decode <file>\n");

    const char* ifn = argv[2];
    FILE* ifp = tnt_fopen(ifn, "r");

    /* Get infile size. */
    fseek(ifp, 0L, SEEK_END);
    size_t in_sz = ftell(ifp);
    tnt_log("%s: %d bytes\n", ifn, in_sz);
    rewind(ifp);

    /* Read file content into buffer. */
    unsigned char* in = (unsigned char*)malloc(in_sz);
    fread(in, 1, in_sz, ifp);
    fclose(ifp);

    /* Compress or decompress. */
    unsigned char* out;
    unsigned int out_sz = 0;
    if (encode) out = ricecodec_compress(in, in_sz, &out_sz);
    else out = ricecodec_decompress(in, in_sz, &out_sz);

    /* Write to outfile. */
    const char* ofn = argv[3];
    FILE* ofp = tnt_fopen(ofn, "w");
    fwrite(out, 1, out_sz, ofp);
    fclose(ofp);

    /* Write (de-)compression ratio. */
    tnt_log("%s: %d bytes (%.2f%%)\n", ofn, out_sz,
            (double)(((double)out_sz / (double)in_sz) * 100));

    free((void*)out);
    free((void*)in);

    return EXIT_SUCCESS;
}

