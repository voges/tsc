#include <stdio.h>
#include <stdlib.h>

static void fwrite_byte(unsigned char byte, FILE* fp)
{
    fwrite(&byte, 1, 1, fp);
}

static void fwrite_uint(unsigned int buf, FILE* fp)
{
    fwrite_byte((buf >> 24) & 0xFF, fp);
    fwrite_byte((buf >> 16) & 0xFF, fp);
    fwrite_byte((buf >>  8) & 0xFF, fp);
    fwrite_byte((buf      ) & 0xFF, fp);    
}

static void fread_uint8(unsigned int* buf, FILE* fp)
{
    unsigned char* bytes = (unsigned char*)malloc(4);
    fread(bytes, 4, 1, fp);
    *buf = bytes[0] << 24 | bytes[1] << 16 | bytes[2] << 8 | bytes[3];
    free((void*)bytes);
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("Error: Exactly one argument needed!\n");
        return EXIT_FAILURE;
    }

    const char* fn = argv[1];
    FILE* fp = fopen(fn, "wb");
    while(fgets(fp)) {
        unsigned char* out = ricecodec_compress(in, in_sz, out_sz);
        unsigned char* dec = ricecodec_uncompress(out, out_sz, dec_sz); 
    }
    fclose(fp);

    size_t i = 0;
    for (i = 0; i < in_sz; i++) {
        printf("%c", in[i]);
        printf("%c", dec[i]);
    }

    return EXIT_SUCCESS;
}

