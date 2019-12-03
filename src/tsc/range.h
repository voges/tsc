//
// Note it is up to the calling code to ensure that no overruns on input and
// output buffers occur.
//

#ifndef RANGE_H
#define RANGE_H

unsigned char *range_compress_o0(unsigned char *in, unsigned int in_sz, unsigned int *out_sz);
unsigned char *range_decompress_o0(unsigned char *in,
                                   // unsigned int   in_sz,
                                   unsigned int *out_sz);
unsigned char *range_compress_o1(unsigned char *in, unsigned int in_sz, unsigned int *out_sz);
unsigned char *range_decompress_o1(unsigned char *in,
                                   // unsigned int  in_sz,
                                   unsigned int *out_sz);

#endif  // RANGE_H
