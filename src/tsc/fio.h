// Copyright 2015 Leibniz University Hannover (LUH)

#ifndef TSC_FIO_H_
#define TSC_FIO_H_

#include <stdint.h>
#include <stdio.h>

FILE *tsc_fopen(const char *fname, const char *mode);

void tsc_fclose(FILE *fp);

size_t tsc_fwrite_byte(FILE *fp, unsigned char byte);

size_t tsc_fwrite_buf(FILE *fp, const unsigned char *buf, size_t n);

size_t tsc_fwrite_uint64(FILE *fp, uint64_t qword);

size_t tsc_fread_byte(FILE *fp, unsigned char *byte);

size_t tsc_fread_buf(FILE *fp, unsigned char *buf, size_t n);

size_t tsc_fread_uint64(FILE *fp, uint64_t *qword);

#endif  // TSC_FIO_H_
