//uint64_t *lut       = (uint64_t *)tsc_malloc(0);
//size_t lut_sz       = 0;
//size_t lut_itr      = 0;

// LUT
//unsigned char lut_magic[8] = "lut----"; lut_magic[7] = '\0';
//uint64_t lut_sz = 0;

// Retain information for LUT entry
//lut_sz += 7 * sizeof(uint64_t);
//lut = (uint64_t *)tsc_realloc(lut, lut_sz);
//lut[lut_itr++] = bh_fpos;
//lut[lut_itr++] = bh_blk_cnt;
//lut[lut_itr++] = bh_rec_cnt;
//lut[lut_itr++] = bh_rec_n;
//lut[lut_itr++] = bh_chr_cnt;
//lut[lut_itr++] = bh_pos_min;
//lut[lut_itr++] = bh_pos_max;

// Write LUT
//unsigned char lut_magic[8] = "lut----"; lut_magic[7] = '\0';
//tsc_sz[TSC_FF] += fwrite_buf(samenc->ofp, lut_magic, sizeof(lut_magic));
//tsc_sz[TSC_FF] += fwrite_uint64(samenc->ofp, lut_sz);
//tsc_sz[TSC_FF] += fwrite_buf(fileenc->ofp, (unsigned char *)lut, lut_sz);
//if (!lut) {
//    tsc_error("Tried to free null pointer\n");
//} else {
//    free(lut);
//    lut = NULL;
//}

// Seek to LUT position
//fseek(filedec->ifp, (long)lut_pos, SEEK_SET);

// LUT header
//unsigned char lut_magic[8];
//uint64_t      lut_sz;

//if (fread_buf(filedec->ifp, lut_magic, sizeof(lut_magic)) != sizeof(lut_magic))
//    tsc_error("Failed to read LUT magic\n");
//if (fread_uint64(filedec->ifp, &lut_sz) != sizeof(lut_sz))
//    tsc_error("Failed to read LUT size\n");
//if (strncmp((const char*)lut_magic, "lut----", 7))
//    tsc_error("Wrong LUT magic!\n");

// Get LUT entries
//uint64_t *lut = (uint64_t *)tsc_malloc(lut_sz);

//if (fread_buf(filedec->ifp, (unsigned char *)lut, lut_sz) != lut_sz)
//    tsc_error("Failed to read LUT entries\n");

//tsc_log("\n"
//        "\tInfo:\n"
//        "\t----\n"
//        "\t        fpos       blk_cnt       rec_cnt"
//        "         rec_n       chr_cnt       pos_min       pos_max\n");

//size_t lut_itr = 0;
//while (lut_itr < (lut_sz / sizeof(uint64_t))) {
//    printf("\t");
//    printf("%12"PRIu64"  ", lut[lut_itr++]);
//    printf("%12"PRIu64"  ", lut[lut_itr++]);
//    printf("%12"PRIu64"  ", lut[lut_itr++]);
//    printf("%12"PRIu64"  ", lut[lut_itr++]);
//    printf("%12"PRIu64"  ", lut[lut_itr++]);
//    printf("%12"PRIu64"  ", lut[lut_itr++]);
//    printf("%12"PRIu64"  ", lut[lut_itr++]);
//    printf("\n");
//}
//printf("\n");

