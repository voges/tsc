// Copyright 2015 Leibniz University Hannover (LUH)

#include "fio.h"

#include <stdlib.h>

#include "log.h"
#include "mem.h"

FILE *tsc_fopen(const char *fname, const char *mode) {
    FILE *fp = fopen(fname, mode);
    if (fp == NULL) {
        tsc_error("Failed to open file: %s\n", fname);
    }
    return fp;
}

void tsc_fclose(FILE *fp) {
    if (fp != NULL) {
        fclose(fp);
    } else {
        tsc_error("Failed to close file\n");
    }
}

size_t tsc_fwrite_byte(FILE *fp, const unsigned char byte) {
    if (fwrite(&byte, 1, 1, fp) != 1) {
        tsc_error("Could not write byte\n");
    }
    return 1;
}

size_t tsc_fwrite_buf(FILE *fp, const unsigned char *buf, const size_t n) {
    if (fwrite(buf, 1, n, fp) != n) {
        tsc_error("Could not write %zu byte(s)\n", n);
    }
    return n;
}

size_t tsc_fwrite_uint64(FILE *fp, const uint64_t qword) {
    tsc_fwrite_byte(fp, (unsigned char)(qword >> (uint64_t)56) & (uint8_t)0xFF);
    tsc_fwrite_byte(fp, (unsigned char)(qword >> (uint64_t)48) & (uint8_t)0xFF);
    tsc_fwrite_byte(fp, (unsigned char)(qword >> (uint64_t)40) & (uint8_t)0xFF);
    tsc_fwrite_byte(fp, (unsigned char)(qword >> (uint64_t)32) & (uint8_t)0xFF);
    tsc_fwrite_byte(fp, (unsigned char)(qword >> (uint64_t)24) & (uint8_t)0xFF);
    tsc_fwrite_byte(fp, (unsigned char)(qword >> (uint64_t)16) & (uint8_t)0xFF);
    tsc_fwrite_byte(fp, (unsigned char)(qword >> (uint64_t)8) & (uint8_t)0xFF);
    tsc_fwrite_byte(fp, (unsigned char)(qword) & (uint8_t)0xFF);
    return sizeof(uint64_t);
}

size_t tsc_fread_byte(FILE *fp, unsigned char *byte) { return fread(byte, 1, 1, fp); }

size_t tsc_fread_buf(FILE *fp, unsigned char *buf, const size_t n) { return fread(buf, 1, n, fp); }

size_t tsc_fread_uint64(FILE *fp, uint64_t *qword) {
    unsigned char *bytes = (unsigned char *)malloc(sizeof(uint64_t));
    if (!bytes) abort();
    size_t ret = fread(bytes, 1, sizeof(uint64_t), fp);

    if (ret != sizeof(uint64_t)) {
        tsc_free(bytes);
        return ret;
    }

    *qword = (uint64_t)bytes[0] << (uint64_t)56 | (uint64_t)bytes[1] << (uint64_t)48 |
             (uint64_t)bytes[2] << (uint64_t)40 | (uint64_t)bytes[3] << (uint64_t)32 |
             (uint64_t)bytes[4] << (uint64_t)24 | (uint64_t)bytes[5] << (uint64_t)16 |
             (uint64_t)bytes[6] << (uint64_t)8 | (uint64_t)bytes[7];

    tsc_free(bytes);
    return ret;
}
