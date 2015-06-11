/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#ifndef TSC_RICECODEC_H
#define TSC_RICECODEC_H

unsigned char *ricecodec_compress(unsigned char* in,
                                  unsigned int   in_size,
                                  unsigned int*  out_size);
unsigned char *ricecodec_uncompress(unsigned char* in,
                                    unsigned int   in_size,
                                    unsigned int*  out_size);

#endif /* TSC_RICECODEC_H */

