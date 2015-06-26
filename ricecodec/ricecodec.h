/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of ricecodec.                                            *
 ******************************************************************************/

#ifndef RICECODEC_H
#define RICECODEC_H

#include <stdlib.h>

unsigned char* ricecodec_compress(unsigned char* in,
                                  size_t         in_size,
                                  size_t*        out_size);
unsigned char* ricecodec_decompress(unsigned char* in,
                                    size_t         in_size,
                                    size_t*        out_size);

#endif /* RICECODEC_H */

