//
// Copyright (c) 2015
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
//

//
// This file is part of tsc.
//
// Tsc is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Tsc is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with tsc. If not, see <http://www.gnu.org/licenses/>
//

#include "tscfmt.h"
#include "frw/frw.h"
#include "tsclib.h"
#include <inttypes.h>

size_t tscfmt_write_fh(tscfh_t *tscfh, FILE *fp)
{
    size_t ret = 0;

    ret += fwrite_buf(fp, tscfh->magic, sizeof(tscfh->magic));
    ret += fwrite_byte(fp, tscfh->flags);
    ret += fwrite_buf(fp, tscfh->ver, sizeof(tscfh->ver));
    ret += fwrite_uint64(fp, tscfh->rec_n);
    ret += fwrite_uint64(fp, tscfh->blk_n);
    ret += fwrite_uint64(fp, tscfh->sblk_m);

    tsc_vlog("Wrote tsc file header version %s\n", tscfh->ver);

    return ret;
}

size_t tscfmt_write_sh(tscsh_t *tscsh, FILE *fp)
{
    size_t ret = 0;

    ret += fwrite_uint64(fp, tscsh->data_sz);
    ret += fwrite_buf(fp, tscsh->data, tscsh->data_sz);

    tsc_vlog("Wrote SAM header\n");

    return ret;
}

size_t tscfmt_write_bh(tscbh_t *tscbh, FILE *fp)
{
    size_t ret = 0;

    ret += fwrite_uint64(fp, tscbh->fpos);
    ret += fwrite_uint64(fp, tscbh->fpos_nxt);
    ret += fwrite_uint64(fp, tscbh->blk_cnt);
    ret += fwrite_uint64(fp, tscbh->rec_cnt);
    ret += fwrite_uint64(fp, tscbh->rec_n);
    ret += fwrite_uint64(fp, tscbh->chr_cnt);
    ret += fwrite_uint64(fp, tscbh->pos_min);
    ret += fwrite_uint64(fp, tscbh->pos_max);

    return ret;
}

void tscfmt_read_fh(tscfh_t *tscfh, FILE *fp)
{
    fread_buf(fp, tscfh->magic, sizeof(tscfh->magic));
    fread_byte(fp, &(tscfh->flags));
    fread_buf(fp, tscfh->ver, sizeof(tscfh->ver));
    fread_uint64(fp, &(tscfh->rec_n));
    fread_uint64(fp, &(tscfh->blk_n));
    fread_uint64(fp, &(tscfh->sblk_m));

    tsc_log("Version: %s\n", tscfh->magic, tscfh->ver);
    tsc_log("Number of blocks: %"PRIu64"\n", tscfh->blk_n);
}

void tscfmt_read_sh(tscsh_t *tscsh, FILE *fp)
{
    fread_uint64(fp, &(tscsh->data_sz));
    tscsh->data = (unsigned char *)tsc_malloc((size_t)tscsh->data_sz);
    fread_buf(fp, tscsh->data, tscsh->data_sz);
    // TODO: free?
}

void tscfmt_read_bh(tscbh_t *tscbh, FILE *fp)
{
    fread_uint64(fp, &(tscbh->fpos));
    fread_uint64(fp, &(tscbh->fpos_nxt));
    fread_uint64(fp, &(tscbh->blk_cnt));
    fread_uint64(fp, &(tscbh->rec_cnt));
    fread_uint64(fp, &(tscbh->rec_n));
    fread_uint64(fp, &(tscbh->chr_cnt));
    fread_uint64(fp, &(tscbh->pos_min));
    fread_uint64(fp, &(tscbh->pos_max));
}

