/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_ARITHCODEC_H
#define TSC_ARITHCODEC_H

unsigned char *arithcodec_compress_O0(unsigned char *in, unsigned int in_size, unsigned int *out_size);
unsigned char *arithcodec_uncompress_O0(unsigned char *in, unsigned int in_size, unsigned int *out_size);
unsigned char *arithcodec_compress_O1(unsigned char *in, unsigned int in_size, unsigned int *out_size);
unsigned char *arithcodec_uncompress_O1(unsigned char *in, unsigned int in_size, unsigned int *out_size);

#endif /* TSC_ARITHCODEC_H */

