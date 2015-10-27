//
// Copyright (c) 2015
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
//

#ifndef CRC64_H
#define CRC64_H

#include <stdint.h>
#include <stdlib.h>

uint64_t crc64(const unsigned char *buf, size_t buf_sz);

#endif // CRC64_H

