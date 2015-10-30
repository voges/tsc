//
// Copyright (c) 2015
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
//

//
// Note it is up to the calling code to ensure that no overruns on input and
// output buffers occur.
//

#ifndef RICE_H
#define RICE_H

#include <stdlib.h>

unsigned char * rice_compress(unsigned char *in,
                              size_t        in_sz,
                              size_t        *out_sz);
unsigned char * rice_decompress(unsigned char *in,
                                size_t        in_sz,
                                size_t        *out_sz);

#endif // RICE_H

