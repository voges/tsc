/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_AUXENC_H
#define TSC_AUXENC_H

unsigned char *arith_compress_O0(unsigned char *in, unsigned int in_size, unsigned int *out_size);
unsigned char *arith_uncompress_O0(unsigned char *in, unsigned int in_size, unsigned int *out_size);
unsigned char *arith_compress_O1(unsigned char *in, unsigned int in_size, unsigned int *out_size);
unsigned char *arith_uncompress_O1(unsigned char *in, unsigned int in_size, unsigned int *out_size);

#endif /* TSC_ACCODEC_H */

