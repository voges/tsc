#include "tscfmt.h"
#include <inttypes.h>
#include <string.h>
#include "fio.h"
#include "log.h"
#include "mem.h"

// File header
static void tscfh_init(tscfh_t *tscfh) {
    tscfh->magic[0] = 't';
    tscfh->magic[1] = 's';
    tscfh->magic[2] = 'c';
    tscfh->magic[3] = '\0';
    tscfh->flags = 0x0;
    // tscfh->ver[0]    = TSC_VERSION_MAJOR + 48; // ASCII offset
    // tscfh->ver[1]    = '.';
    // tscfh->ver[2]    = TSC_VERSION_MINOR + 48;
    // tscfh->ver[3]    = '.';
    // tscfh->ver[4]    = TSC_VERSION_PATCH + 48;
    // tscfh->ver[5]    = '\0';
    tscfh->rec_n = 0;
    tscfh->blk_n = 0;
    tscfh->sblk_n = 0;
}

tscfh_t *tscfh_new(void) {
    tscfh_t *tscfh = (tscfh_t *)tsc_malloc(sizeof(tscfh_t));
    tscfh_init(tscfh);
    return tscfh;
}

void tscfh_free(tscfh_t *tscfh) {
    if (tscfh != NULL) {
        free(tscfh);
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

size_t tscfh_read(tscfh_t *tscfh, FILE *fp) {
    size_t ret = 0;

    ret += tsc_fread_buf(fp, tscfh->magic, sizeof(tscfh->magic));
    ret += tsc_fread_byte(fp, &(tscfh->flags));
    // ret += tsc_fread_buf(fp, tscfh->ver, sizeof(tscfh->ver));
    ret += tsc_fread_uint64(fp, &(tscfh->rec_n));
    ret += tsc_fread_uint64(fp, &(tscfh->blk_n));
    ret += tsc_fread_uint64(fp, &(tscfh->sblk_n));

    // Sanity check
    if (strncmp((const char *)tscfh->magic, "tsc", 3) != 0)
        tsc_error("File magic does not match\n");
    if (!(tscfh->flags & (uint8_t)0x1))
        tsc_error("File seems not to contain data in SAM format\n");
    // if (strncmp(tsc_version->s, (const char *)tscfh->ver, tsc_version->len))
    //     tsc_error("File was compressed with another version\n");
    if (!(tscfh->rec_n)) tsc_error("File does not contain any records\n");
    if (!(tscfh->blk_n)) tsc_error("File does not contain any blocks\n");
    if (!(tscfh->sblk_n)) tsc_error("File does not contain sub-blocks\n");

    // printf("Read tsc file header\n");

    return ret;
}

size_t tscfh_write(tscfh_t *tscfh, FILE *fp) {
    size_t ret = 0;

    ret += tsc_fwrite_buf(fp, tscfh->magic, sizeof(tscfh->magic));
    ret += tsc_fwrite_byte(fp, tscfh->flags);
    // ret += tsc_fwrite_buf(fp, tscfh->ver, sizeof(tscfh->ver));
    // printf("%zu %s\n", sizeof(tscfh->ver), tscfh->ver);
    ret += tsc_fwrite_uint64(fp, tscfh->rec_n);
    ret += tsc_fwrite_uint64(fp, tscfh->blk_n);
    ret += tsc_fwrite_uint64(fp, tscfh->sblk_n);

    // printf("Wrote tsc file header\n");

    return ret;
}

size_t tscfh_size(tscfh_t *tscfh) {
    return sizeof(tscfh->magic) +
           sizeof(tscfh->flags)
           // + sizeof(tscfh->ver)
           + sizeof(tscfh->rec_n) + sizeof(tscfh->blk_n) +
           sizeof(tscfh->sblk_n);
}

// SAM header
static void tscsh_init(tscsh_t *tscsh) {
    tscsh->data_sz = 0;
    tscsh->data = NULL;
}

tscsh_t *tscsh_new(void) {
    tscsh_t *tscsh = (tscsh_t *)tsc_malloc(sizeof(tscsh_t));
    tscsh_init(tscsh);
    return tscsh;
}

void tscsh_free(tscsh_t *tscsh) {
    if (tscsh != NULL) {
        if (tscsh->data != NULL) {
            free(tscsh->data);
            tscsh->data = NULL;
        }
        free(tscsh);
        // tscsh = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

size_t tscsh_read(tscsh_t *tscsh, FILE *fp) {
    size_t ret = 0;

    ret += tsc_fread_uint64(fp, &(tscsh->data_sz));
    tscsh->data = (unsigned char *)tsc_malloc((size_t)tscsh->data_sz);
    ret += tsc_fread_buf(fp, tscsh->data, tscsh->data_sz);

    // printf("Read SAM header\n");

    return ret;
}

size_t tscsh_write(tscsh_t *tscsh, FILE *fp) {
    if (tscsh->data_sz == 0 || tscsh->data == NULL) {
        tsc_log("Attention: empty SAM header\n");
        return 0;
    }

    size_t ret = 0;

    ret += tsc_fwrite_uint64(fp, tscsh->data_sz);
    ret += tsc_fwrite_buf(fp, tscsh->data, tscsh->data_sz);

    // printf("Wrote SAM header\n");

    return ret;
}

// size_t tscsh_size(tscsh_t *tscsh)
// {
//     return sizeof(tscsh->data_sz) + sizeof(tscsh->data);
// }

// Block header
static void tscbh_init(tscbh_t *tscbh) {
    tscbh->fpos = 0;
    tscbh->fpos_nxt = 0;
    tscbh->blk_cnt = 0;
    tscbh->rec_cnt = 0;
    tscbh->rec_max = 0;
    tscbh->rec_cnt = 0;
    tscbh->rname = 0;
    tscbh->pos_min = 0;
    tscbh->pos_max = 0;
}

tscbh_t *tscbh_new(void) {
    tscbh_t *tscbh = (tscbh_t *)tsc_malloc(sizeof(tscbh_t));
    tscbh_init(tscbh);
    return tscbh;
}

void tscbh_free(tscbh_t *tscbh) {
    if (tscbh != NULL) {
        free(tscbh);
        // tscbh = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

size_t tscbh_read(tscbh_t *tscbh, FILE *fp) {
    size_t ret = 0;

    ret += tsc_fread_uint64(fp, &(tscbh->fpos));
    ret += tsc_fread_uint64(fp, &(tscbh->fpos_nxt));
    ret += tsc_fread_uint64(fp, &(tscbh->blk_cnt));
    ret += tsc_fread_uint64(fp, &(tscbh->rec_cnt));
    ret += tsc_fread_uint64(fp, &(tscbh->rec_max));
    ret += tsc_fread_uint64(fp, &(tscbh->rname));
    ret += tsc_fread_uint64(fp, &(tscbh->pos_min));
    ret += tsc_fread_uint64(fp, &(tscbh->pos_max));

    // printf("Read block header %"PRIu64"\n", tscbh->blk_cnt);

    return ret;
}

size_t tscbh_write(tscbh_t *tscbh, FILE *fp) {
    size_t ret = 0;

    ret += tsc_fwrite_uint64(fp, tscbh->fpos);
    ret += tsc_fwrite_uint64(fp, tscbh->fpos_nxt);
    ret += tsc_fwrite_uint64(fp, tscbh->blk_cnt);
    ret += tsc_fwrite_uint64(fp, tscbh->rec_cnt);
    ret += tsc_fwrite_uint64(fp, tscbh->rec_max);
    ret += tsc_fwrite_uint64(fp, tscbh->rname);
    ret += tsc_fwrite_uint64(fp, tscbh->pos_min);
    ret += tsc_fwrite_uint64(fp, tscbh->pos_max);

    // printf("Wrote block header %"PRIu64"\n", tscbh->blk_cnt);

    return ret;
}

// size_t tscbh_size(tscbh_t *tscbh)
// {
//     return   sizeof(tscbh->fpos)
//            + sizeof(tscbh->fpos_nxt)
//            + sizeof(tscbh->blk_cnt)
//            + sizeof(tscbh->rec_cnt)
//            + sizeof(tscbh->rec_max)
//            + sizeof(tscbh->rname)
//            + sizeof(tscbh->pos_min)
//            + sizeof(tscbh->pos_max);
// }
