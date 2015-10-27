//
// Copyright (c) 2015
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
//

//
// Note it is up to the calling code to ensure that no overruns on input and
// output buffers occur.
//

#ifndef RANGECODEC_H
#define RANGECODEC_H

unsigned char * range_compress_o0(unsigned char *in,
                                  unsigned int  in_sz,
                                  unsigned int  *out_sz);
unsigned char * range_decompress_o0(unsigned char *in,
                                   unsigned int   in_sz,
                                   unsigned int   *out_sz);
unsigned char * range_compress_o1(unsigned char *in,
                                  unsigned int  in_sz,
                                  unsigned int  *out_sz);
unsigned char * range_decompress_o1(unsigned char *in,
                                    unsigned int  in_sz,
                                    unsigned int  *out_sz);

// TODO: possibly replace 'unsigned int' with 'size_t'

#endif // RANGECODEC_H

