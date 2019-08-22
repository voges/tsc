//
// Wrapper functions to safely read/write different data types from/to files (or
// streams).
// The 'tsc_fwrite_uintXX' and 'tsc_fread_uintXX' functions are compatible
// with each other. They are independent from the endianness of the system this
// code is built on.
//

#ifndef TSC_FIO_H
#define TSC_FIO_H

#include <stdint.h>
#include <stdio.h>

FILE *tsc_fopen(const char *fname, const char *mode);
void tsc_fclose(FILE *fp);

size_t tsc_fwrite_byte(FILE *fp, unsigned char byte);
size_t tsc_fwrite_buf(FILE *fp, const unsigned char *buf, size_t n);
// size_t tsc_fwrite_uint32(FILE *fp, const uint32_t dword);
size_t tsc_fwrite_uint64(FILE *fp, uint64_t qword);

size_t tsc_fread_byte(FILE *fp, unsigned char *byte);
size_t tsc_fread_buf(FILE *fp, unsigned char *buf, size_t n);
// size_t tsc_fread_uint32(FILE *fp, uint32_t *dword);
size_t tsc_fread_uint64(FILE *fp, uint64_t *qword);

#endif  // TSC_FIO_H
