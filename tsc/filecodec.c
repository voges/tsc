/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "filecodec.h"
#include "frw.h"
#include "tsclib.h"
#include "version.h"
#include <inttypes.h>
#include <string.h>

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void fileenc_init(fileenc_t* fe, FILE* ifp, FILE* ofp,
                         const size_t block_sz)
{
    fe->ifp = ifp;
    fe->ofp = ofp;
    fe->block_sz = block_sz;
}

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp, const size_t block_sz)
{
    fileenc_t* fe = (fileenc_t *)tsc_malloc(sizeof(fileenc_t));
    fe->samparser = samparser_new(ifp);
    fe->nucenc = nucenc_new();
    fe->qualenc = qualenc_new();
    fe->auxenc = auxenc_new();
    fe->stats = str_new();
    fileenc_init(fe, ifp, ofp, block_sz);
    return fe;
}

void fileenc_free(fileenc_t* fe)
{
    if (fe != NULL) {
        samparser_free(fe->samparser);
        nucenc_free(fe->nucenc);
        qualenc_free(fe->qualenc);
        auxenc_free(fe->auxenc);
        str_free(fe->stats);
        free(fe);
        fe = NULL;
    } else { /* fe == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

str_t* fileenc_encode(fileenc_t* fe)
{
    /* Statistics. */
    uint32_t block_cnt   = 0; /* block counter                              */
    uint32_t line_cnt    = 0; /* line counter                               */
    size_t byte_cnt      = 0; /* total no. of bytes written                 */
    size_t ff_byte_cnt   = 0; /* total no. of bytes written for file format */
    size_t sh_byte_cnt   = 0; /* total no. of bytes written for SAM header  */
    size_t nuc_byte_cnt  = 0; /* total no. of bytes written by nucenc       */
    size_t qual_byte_cnt = 0; /* total no. of bytes written by qualenc      */
    size_t aux_byte_cnt  = 0; /* total no. of bytes written by auxenc       */

    /* Write tsc file header:
     * - unsigned char[7] : "tsc----"
     * - unsigned char[5] : VERSION
     * - uint64_t         : block size
     */
    tsc_log("Format: %s %s\n", tsc_prog_name->s, tsc_version->s);
    ff_byte_cnt += fwrite_cstr(fe->ofp, tsc_prog_name->s);
    ff_byte_cnt += fwrite_cstr(fe->ofp, "----");
    ff_byte_cnt += fwrite_cstr(fe->ofp, tsc_version->s);
    tsc_log("Block size: %"PRIu64"\n", fe->block_sz);
    ff_byte_cnt += fwrite_uint64(fe->ofp, (uint64_t)fe->block_sz);

    /* Write SAM header:
     * - uint64_t       : header size
     * - unsigned char[]: data
     */
    if (tsc_verbose) tsc_log("Writing SAM header\n");
    ff_byte_cnt += fwrite_uint64(fe->ofp, (uint64_t)fe->samparser->head->len);
    sh_byte_cnt += fwrite_cstr(fe->ofp, fe->samparser->head->s);

    /* Parse SAM file line by line and invoke encoders. */
    uint32_t block_line_cnt = 0; /* block-wise line count */
    samrecord_t* samrecord = &(fe->samparser->curr);

    while (samparser_next(fe->samparser)) {
        if (block_line_cnt >= fe->block_sz) {
            /* Write block header:
             * - uint32_t: block count
             * - uint32_t: no. of lines in block
             */
            if (tsc_verbose) tsc_log("Writing block %zu: %zu line(s)\n", block_cnt, block_line_cnt);
            ff_byte_cnt += fwrite_uint32(fe->ofp, block_cnt);
            ff_byte_cnt += fwrite_uint32(fe->ofp, block_line_cnt);

            /* Write NUC, QUAL, and AUX blocks. */
            nuc_byte_cnt += nucenc_write_block(fe->nucenc, fe->ofp);
            qual_byte_cnt += qualenc_write_block(fe->qualenc, fe->ofp);
            aux_byte_cnt += auxenc_write_block(fe->auxenc, fe->ofp);

            block_cnt++;
            block_line_cnt = 0;
        }

        /* Add records to encoders. */
        nucenc_add_record(fe->nucenc,
                          samrecord->int_fields[POS],
                          samrecord->str_fields[CIGAR],
                          samrecord->str_fields[SEQ]);
        qualenc_add_record(fe->qualenc,
                           samrecord->str_fields[QUAL]);
        auxenc_add_record(fe->auxenc,
                          samrecord->str_fields[QNAME],
                          samrecord->int_fields[FLAG],
                          samrecord->str_fields[RNAME],
                          samrecord->int_fields[MAPQ],
                          samrecord->str_fields[RNEXT],
                          samrecord->int_fields[PNEXT],
                          samrecord->int_fields[TLEN],
                          samrecord->str_fields[OPT]);
        block_line_cnt++;
        line_cnt++;
    }

    /* Write last block header:
     * - uint32_t: block count
     * - uint32_t: no. of lines in block
     */
    if (tsc_verbose) tsc_log("Writing last block %"PRIu32": %"PRIu32" line(s)\n", block_cnt, block_line_cnt);
    ff_byte_cnt += fwrite_uint32(fe->ofp, block_cnt);
    ff_byte_cnt += fwrite_uint32(fe->ofp, block_line_cnt);

    /* Write last NUC, QUAL, and AUX blocks. */
    nuc_byte_cnt += nucenc_write_block(fe->nucenc, fe->ofp);
    qual_byte_cnt += qualenc_write_block(fe->qualenc, fe->ofp);
    aux_byte_cnt += auxenc_write_block(fe->auxenc, fe->ofp);

    block_cnt++;
    byte_cnt = ff_byte_cnt + sh_byte_cnt + nuc_byte_cnt + qual_byte_cnt + aux_byte_cnt;
    tsc_log("Wrote %zu bytes ~= %.2f GiB (%zu line(s) in %zu block(s))\n", byte_cnt, ((double)byte_cnt / GB), line_cnt, block_cnt);

    /* Return compression statistics. */
    char stats[4 * KB]; /* this should be enough space */
    snprintf(stats, sizeof(stats),
             "\tCompression Statistics:\n"
             "\t-----------------------\n"
             "\tNumber of blocks written  : %12zu\n"
             "\tNumber of lines processed : %12zu\n"
             "\tBytes written             : %12zu (%6.2f%%)\n"
             "\t  File format             : %12zu (%6.2f%%)\n"
             "\t  SAM header              : %12zu (%6.2f%%)\n"
             "\t  NUC                     : %12zu (%6.2f%%)\n"
             "\t  QUAL                    : %12zu (%6.2f%%)\n"
             "\t  AUX                     : %12zu (%6.2f%%)\n",
             (size_t)block_cnt,
             (size_t)line_cnt,
             byte_cnt, (100 * (double)byte_cnt / (double)byte_cnt),
             ff_byte_cnt, (100 * (double)ff_byte_cnt / (double)byte_cnt),
             sh_byte_cnt, (100 * (double)sh_byte_cnt / (double)byte_cnt),
             nuc_byte_cnt, (100 * (double)nuc_byte_cnt / (double)byte_cnt),
             qual_byte_cnt, (100 * (double)qual_byte_cnt / (double)byte_cnt),
             aux_byte_cnt, (100 * (double)aux_byte_cnt / (double)byte_cnt));
    str_append_cstr(fe->stats, stats);

    return fe->stats;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void filedec_init(filedec_t* fd, FILE* ifp, FILE* ofp)
{
    fd->ifp = ifp;
    fd->ofp = ofp;
}

filedec_t* filedec_new(FILE* ifp, FILE* ofp)
{
    filedec_t* fd = (filedec_t *)tsc_malloc(sizeof(filedec_t));
    fd->nucdec = nucdec_new();
    fd->qualdec = qualdec_new();
    fd->auxdec = auxdec_new();
    fd->stats = str_new();
    filedec_init(fd, ifp, ofp);
    return fd;
}

void filedec_free(filedec_t* fd)
{
    if (fd != NULL) {
        nucdec_free(fd->nucdec);
        qualdec_free(fd->qualdec);
        auxdec_free(fd->auxdec);
        str_free(fd->stats);
        free(fd);
        fd = NULL;
    } else { /* fd == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

str_t* filedec_decode(filedec_t* fd)
{
    /* Statistics. */
    uint32_t line_cnt = 0; /* total no. of lines processed */
    size_t byte_cnt   = 0; /* total no. of bytes written   */

    /* Read tsc file header:
     * - unsigned char[7] : "tsc----"
     * - unsigned char[5] : VERSION
     * - uint64_t         : block size
     */
    unsigned char prog_name[7];
    unsigned char version[5];
    uint64_t block_sz;

    if (fread_buf(fd->ifp, prog_name, sizeof(prog_name)) != sizeof(prog_name))
        tsc_error("Failed to read program name!\n");
    if (fread_buf(fd->ifp, version, sizeof(version)) != sizeof(version))
        tsc_error("Failed to read version!\n");
    if (strncmp((const char*)version, (const char*)tsc_version->s, sizeof(version)))
        tsc_error("File was compressed with another version: %s\n", version);
    if (fread_uint64(fd->ifp, &block_sz) != sizeof(block_sz))
        tsc_error("Failed to read block size!\n");

    prog_name[3] = '\0';
    tsc_log("Format: %s %s\n", prog_name, version);
    tsc_log("Block size: %"PRIu64"\n", block_sz);

    /* Write SAM header:
     * - uint64_t       : header size
     * - unsigned char[]: data
     */
    if (tsc_verbose) tsc_log("Reading SAM header\n");
    uint64_t header_sz;
    if (fread_uint64(fd->ifp, &header_sz) != sizeof(header_sz))
        tsc_error("Failed to read SAM header size!\n");
    str_t* header = str_new();
    str_reserve(header, header_sz);
    if (fread_buf(fd->ifp, (unsigned char*)header->s, header_sz) != header_sz)
        tsc_error("Failed to read SAM header!\n");
    byte_cnt += fwrite_cstr(fd->ofp, header->s);
    str_free(header);

    /* Read block header:
     * - uint32_t: block count
     * - uint32_t: no. of lines in block
     */
    uint32_t block_cnt = 0;
    uint32_t block_line_cnt = 0;

    while (fread_uint32(fd->ifp, &block_cnt) == sizeof(block_cnt)) {
        if (fread_uint32(fd->ifp, &block_line_cnt) != sizeof(block_line_cnt))
            tsc_error("Failed to read number of lines in block %"PRIu32"!\n", block_cnt);
        if (tsc_verbose) tsc_log("Decoding block %"PRIu32": %"PRIu32" lines\n", block_cnt, block_line_cnt);

        /* Allocate memory to prepare decoding of the next block. */
        uint64_t* pos = (uint64_t*)tsc_malloc(sizeof(uint64_t) * block_line_cnt);
        str_t** cigar = (str_t**)tsc_malloc(sizeof(str_t*) * block_line_cnt);
        str_t** seq = (str_t**)tsc_malloc(sizeof(str_t*) * block_line_cnt);
        str_t** qual = (str_t**)tsc_malloc(sizeof(str_t*) * block_line_cnt);
        str_t** aux = (str_t**)tsc_malloc(sizeof(str_t*) * block_line_cnt);

        size_t i = 0;
        for (i = 0; i < block_line_cnt; i++) {
            cigar[i] = str_new();
            seq[i] = str_new();
            qual[i] = str_new();
            aux[i] = str_new();
        }

        /* Decode NUC, QUAL, and AUX blocks. */
        nucdec_decode_block(fd->nucdec, fd->ifp, pos, cigar, seq);
        qualdec_decode_block(fd->qualdec, fd->ifp, qual);
        auxdec_decode_block(fd->auxdec, fd->ifp, aux);

        /* Write NUC, QUAL and AUX in correct order to outfile. */
        if (tsc_verbose) tsc_log("Writing decoded block %"PRIu32": %zu lines\n", block_cnt, block_line_cnt);
        for (i = 0; i < block_line_cnt; i++) {
            enum { QNAME, FLAG, RNAME, POS, MAPQ, CIGAR,
                   RNEXT, PNEXT, TLEN, SEQ, QUAL, OPT };
            char* sam_fields[12];

            /* Get pointers to POS, CIGAR, SEQ, QUAL and AUX. */
            char pos_cstr[101]; /* this should be enough space */
            snprintf(pos_cstr, sizeof(pos_cstr), "%"PRIu64, pos[i]);
            sam_fields[POS] = pos_cstr;
            sam_fields[CIGAR] = cigar[i]->s;
            sam_fields[SEQ] = seq[i]->s;
            sam_fields[QUAL] = qual[i]->s;
            char* aux_itr = aux[i]->s;
            sam_fields[QNAME]= aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[FLAG] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[RNAME] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[MAPQ] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[RNEXT] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[PNEXT] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[TLEN] = ++aux_itr;
            aux_itr = strchr(aux_itr, '\t'); *aux_itr = '\0';
            sam_fields[OPT] = ++aux_itr;

            /* Write data to file. */
            size_t f = 0;
            for (f = 0; f < 12; f++) {
                byte_cnt += fwrite_cstr(fd->ofp, sam_fields[f]);
                byte_cnt += fwrite_byte(fd->ofp, '\t');
            }
            byte_cnt += fwrite_byte(fd->ofp, '\n');

            /* Free the memory used for the current line. */
            str_free(cigar[i]);
            str_free(seq[i]);
            str_free(qual[i]);
            str_free(aux[i]);
        }

        /* Free memory allocated for this block. */
        free((void*)pos);
        free((void*)cigar);
        free((void*)seq);
        free((void*)qual);
        free((void*)aux);

        line_cnt += block_line_cnt;
        block_cnt++;
    }

    tsc_log("Decoded %"PRIu32" line(s) in %"PRIu32" block(s) and wrote %zu bytes ~= %.2f GiB\n",
            line_cnt, block_cnt, byte_cnt, ((double)byte_cnt / GB));

    /* Return decompression statistics. */
    char stats[4*KB]; /* this should be enough space */
    snprintf(stats, sizeof(stats),
             "\tDecompression Statistics:\n"
             "\t-------------------------\n"
             "\tNumber of blocks decoded     : %12zu\n"
             "\tSAM records (lines) processed: %12zu\n"
             "\tTotal bytes written          : %12zu\n",
             (size_t)block_cnt,
             (size_t)line_cnt,
             (size_t)byte_cnt);
    str_append_cstr(fd->stats, stats);

    return fd->stats;
}

