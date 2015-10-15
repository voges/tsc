/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <voges@tnt.uni-hannover.de>
 */

/*
 * Note it is up to the calling code to ensure that no overruns on input and
 * output buffers occur.
 */

#ifndef ARITHCODEC_H
#define ARITHCODEC_H

unsigned char *arith_compress_o0(unsigned char* in,
                                 unsigned int   in_size,
                                 unsigned int*  out_size);
unsigned char *arith_decompress_o0(unsigned char* in,
                                   unsigned int   in_size,
                                   unsigned int*  out_size);
unsigned char *arith_compress_o1(unsigned char* in,
                                 unsigned int   in_size,
                                 unsigned int*  out_size);
unsigned char *arith_decompress_o1(unsigned char* in,
                                   unsigned int   in_size,
                                   unsigned int*  out_size);

#endif /* ARITHCODEC_H */

