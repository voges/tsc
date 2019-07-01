//
// Tsc file format:
// ----------------
//   * Tsc stores the SAM header as plain ASCII text.
//   * Tsc uses dedicated sub-block codecs which generate 'sub-blocks'.
//   * For fast random-access and for minimizing fseek operations, the position
//   * of the next block is stored in each block header.
//
//   [File header            ]
//   [SAM header             ]
//   [Block header 0         ]
//     [Sub-block 0.1        ]
//     [Sub-block 0.2        ]
//     ...
//     [Sub-block 0.(M-1)    ]
//   ...
//   [Block header (N-1)     ]
//     [Sub-block (N-1).1    ]
//     [Sub-block (N-1).2    ]
//     ...
//     [Sub-block (N-1).(M-1)]
//

#ifndef TSC_TSCFMT_H
#define TSC_TSCFMT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// File header
typedef struct tscfh_t_ {
    unsigned char magic[4];  // "tsc----" + '\0'
    uint8_t       flags;     // flags
    // unsigned char ver[6];    // Maj.Min.Pat (5 bytes) + '\0'
    uint64_t      rec_n;     // number of records
    uint64_t      blk_n;     // number of blocks
    uint64_t      sblk_n;    // number of sub-blocks per block
} tscfh_t;

tscfh_t * tscfh_new(void);
void tscfh_free(tscfh_t *tscfh);
size_t tscfh_read(tscfh_t *tscfh, FILE *fp);
size_t tscfh_write(tscfh_t *tscfh, FILE *fp);
size_t tscfh_size(tscfh_t * tscfh);

// SAM header
typedef struct tscsh_t_ {
    uint64_t      data_sz; // SAM header size
    unsigned char *data;   // SAM header data
} tscsh_t;

tscsh_t * tscsh_new(void);
void tscsh_free(tscsh_t *tscsh);
size_t tscsh_read(tscsh_t *tscsh, FILE *fp);
size_t tscsh_write(tscsh_t *tscsh, FILE *fp);
// size_t tscsh_size(tscsh_t * tscsh);

// Block header
typedef struct tscbh_t_ {
    uint64_t fpos;     // fp offset to the beginning of -this- block
    uint64_t fpos_nxt; // fp offset to the beginning of the -next- block
                       // (the last block has all zeros here)
    uint64_t blk_cnt;  // block count, starting with 0
    uint64_t rec_cnt;  // no. of records in this block
    uint64_t rec_max;  // max no. of records in this block
    uint64_t rname;    // RNAME
    uint64_t pos_min;  // smallest POSition contained in block
    uint64_t pos_max;  // largest POSition contained in block
} tscbh_t;

tscbh_t * tscbh_new(void);
void tscbh_free(tscbh_t *tscbh);
size_t tscbh_read(tscbh_t *tscbh, FILE *fp);
size_t tscbh_write(tscbh_t *tscbh, FILE *fp);
// size_t tscbh_size(tscbh_t * tscbh);

#endif // TSC_TSCFMT_H
