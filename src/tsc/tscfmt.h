// Copyright 2015 Leibniz University Hannover (LUH)

//
// TSC file format:
// ----------------
//   - TSC stores the SAM header as plain ASCII text.
//   - TSC uses dedicated sub-block codecs which generate 'sub-blocks'.
//   - For fast random-access and for minimizing seek operations, the position
//   - of the next block is stored in each block header.
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

#ifndef TSC_TSCFMT_H_
#define TSC_TSCFMT_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// File header
// -----------------------------------------------------------------------------

typedef struct tscfh_t_ {
    unsigned char magic[4];
    uint8_t flags;
    uint64_t rec_n;
    uint64_t blk_n;
    uint64_t sblk_n;
} tscfh_t;

tscfh_t *tscfh_new();

void tscfh_free(tscfh_t *tscfh);

size_t tscfh_read(tscfh_t *tscfh, FILE *fp);

size_t tscfh_write(tscfh_t *tscfh, FILE *fp);

size_t tscfh_size(tscfh_t *tscfh);

// SAM header
// -----------------------------------------------------------------------------

typedef struct tscsh_t_ {
    uint64_t data_sz;
    unsigned char *data;
} tscsh_t;

tscsh_t *tscsh_new();

void tscsh_free(tscsh_t *tscsh);

size_t tscsh_read(tscsh_t *tscsh, FILE *fp);

size_t tscsh_write(tscsh_t *tscsh, FILE *fp);

// Block header
// -----------------------------------------------------------------------------

typedef struct tscbh_t_ {
    uint64_t fpos;
    uint64_t fpos_nxt;
    uint64_t blk_cnt;
    uint64_t rec_cnt;
    uint64_t rec_max;
    uint64_t rname;
    uint64_t pos_min;
    uint64_t pos_max;
} tscbh_t;

tscbh_t *tscbh_new();

void tscbh_free(tscbh_t *tscbh);

size_t tscbh_read(tscbh_t *tscbh, FILE *fp);

size_t tscbh_write(tscbh_t *tscbh, FILE *fp);

#endif  // TSC_TSCFMT_H_
