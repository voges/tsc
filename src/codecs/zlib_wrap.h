#ifndef TSC_ZLIB_WRAP_H
#define TSC_ZLIB_WRAP_H

unsigned char * zlib_compress(unsigned char *in,
                              size_t        in_sz,
                              size_t        *out_sz);
unsigned char * zlib_decompress(unsigned char *in,
                                size_t        in_sz,
                                size_t        out_sz);

#endif // TSC_ZLIB_WRAP_H

