#ifndef TSC_CRC64_H
#define TSC_CRC64_H

#include <stdint.h>
#include <stdlib.h>

uint64_t crc64(const unsigned char *buf, size_t buf_sz);

#endif  // TSC_CRC64_H
