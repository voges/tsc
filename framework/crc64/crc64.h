/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of crc64.                                                *
 ******************************************************************************/

#ifndef CRC64_H
#define CRC64_H

#include <stdint.h>
#include <stdlib.h>

uint64_t crc64(const unsigned char* buf, size_t size);

#endif /* CRC64_H */

