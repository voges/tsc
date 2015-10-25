/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of ricecodec.                                            *
 ******************************************************************************/

/*
 * Note it is up to the calling code to ensure that no overruns on input and
 * output buffers occur.
 */

#ifndef RICECODEC_H
#define RICECODEC_H

#include <stdlib.h>

unsigned char* rice_compress(unsigned char* in,
                             size_t         in_size,
                             size_t*        out_size);
unsigned char* rice_decompress(unsigned char* in,
                               size_t         in_size,
                               size_t*        out_size);

#endif /* RICECODEC_H */

