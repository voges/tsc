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
static void fileenc_init(fileenc_t* fileenc, FILE* ifp, FILE* ofp, const uint64_t block_sz)
{
    fileenc->ifp = ifp;
    fileenc->ofp = ofp;
    fileenc->block_sz = block_sz;
}

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp, const uint64_t block_sz)
{
    fileenc_t* fileenc = (fileenc_t *)tsc_malloc(sizeof(fileenc_t));
    fileenc->samparser = samparser_new(ifp);
    fileenc->nucenc = nucenc_new();
    fileenc->qualenc = qualenc_new();
    fileenc->auxenc = auxenc_new();
    fileenc->stats = str_new();
    fileenc_init(fileenc, ifp, ofp, block_sz);
    return fileenc;
}

void fileenc_free(fileenc_t* fileenc)
{
    if (fileenc != NULL) {
        samparser_free(fileenc->samparser);
        nucenc_free(fileenc->nucenc);
        qualenc_free(fileenc->qualenc);
        auxenc_free(fileenc->auxenc);
        str_free(fileenc->stats);
        free((void*)fileenc);
        fileenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

str_t* fileenc_encode(fileenc_t* fileenc)
{
    /* Statistics. */
    uint32_t block_cnt   = 0; /* total no. of blocks written                */
    uint32_t line_cnt    = 0; /* total no. of lines processed               */
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
    ff_byte_cnt += fwrite_cstr(fileenc->ofp, tsc_prog_name->s);
    ff_byte_cnt += fwrite_cstr(fileenc->ofp, "----");
    ff_byte_cnt += fwrite_cstr(fileenc->ofp, tsc_version->s);
    tsc_log("Block size: %"PRIu64"\n", fileenc->block_sz);
    ff_byte_cnt += fwrite_uint64(fileenc->ofp, fileenc->block_sz);

    /* Write SAM header:
     * - uint32_t       : header size
     * - unsigned char[]: data
     */
    if (tsc_verbose) tsc_log("Writing SAM header\n");
    ff_byte_cnt += fwrite_uint32(fileenc->ofp, (uint32_t)fileenc->samparser->head->n);
    sh_byte_cnt += fwrite_cstr(fileenc->ofp, fileenc->samparser->head->s);

    /* Parse SAM file line by line and invoke encoders. */
    uint32_t block_line_cnt = 0; /* block-wise line count */
    samrecord_t* samrecord = &(fileenc->samparser->curr);

    while (samparser_next(fileenc->samparser)) {
        if (block_line_cnt >= fileenc->block_sz) {
            /* Write block header:
             * - uint32_t: block count
             * - uint32_t: no. of lines in block
             */
            if (tsc_verbose) tsc_log("Writing block %zu: %zu line(s)\n", block_cnt, block_line_cnt);
            ff_byte_cnt += fwrite_uint32(fileenc->ofp, block_cnt);
            ff_byte_cnt += fwrite_uint32(fileenc->ofp, block_line_cnt);

            /* Write NUC, QUAL, and AUX blocks. */
            nuc_byte_cnt += nucenc_write_block(fileenc->nucenc, fileenc->ofp);
            qual_byte_cnt += qualenc_write_block(fileenc->qualenc, fileenc->ofp);
            aux_byte_cnt += auxenc_write_block(fileenc->auxenc, fileenc->ofp);

            block_cnt++;
            block_line_cnt = 0;
        }

        /* Add records to encoders. */
        nucenc_add_record(fileenc->nucenc,
                          samrecord->int_fields[POS],
                          samrecord->str_fields[CIGAR],
                          samrecord->str_fields[SEQ]);
        qualenc_add_record(fileenc->qualenc,
                           samrecord->str_fields[QUAL]);
        auxenc_add_record(fileenc->auxenc,
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
    ff_byte_cnt += fwrite_uint32(fileenc->ofp, block_cnt);
    ff_byte_cnt += fwrite_uint32(fileenc->ofp, block_line_cnt);

    /* Write last NUC, QUAL, and AUX blocks. */
    nuc_byte_cnt += nucenc_write_block(fileenc->nucenc, fileenc->ofp);
    qual_byte_cnt += qualenc_write_block(fileenc->qualenc, fileenc->ofp);
    aux_byte_cnt += auxenc_write_block(fileenc->auxenc, fileenc->ofp);

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
             "\t  NUC (SEQ+POS+CIGAR)     : %12zu (%6.2f%%)\n"
             "\t  QUAL                    : %12zu (%6.2f%%)\n"
             "\t  AUX (everything else)   : %12zu (%6.2f%%)\n",
             (size_t)block_cnt,
             (size_t)line_cnt,
             byte_cnt, (100 * (double)byte_cnt / (double)byte_cnt),
             ff_byte_cnt, (100 * (double)ff_byte_cnt / (double)byte_cnt),
             sh_byte_cnt, (100 * (double)sh_byte_cnt / (double)byte_cnt),
             nuc_byte_cnt, (100 * (double)nuc_byte_cnt / (double)byte_cnt),
             qual_byte_cnt, (100 * (double)qual_byte_cnt / (double)byte_cnt),
             aux_byte_cnt, (100 * (double)aux_byte_cnt / (double)byte_cnt));
    str_append_cstr(fileenc->stats, stats);

    return fileenc->stats;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void filedec_init(filedec_t* filedec, FILE* ifp, FILE* ofp)
{
    filedec->ifp = ifp;
    filedec->ofp = ofp;
}

filedec_t* filedec_new(FILE* ifp, FILE* ofp)
{
    filedec_t* filedec = (filedec_t *)tsc_malloc(sizeof(filedec_t));
    filedec->nucdec = nucdec_new();
    filedec->qualdec = qualdec_new();
    filedec->auxdec = auxdec_new();
    filedec->stats = str_new();
    filedec_init(filedec, ifp, ofp);
    return filedec;
}

void filedec_free(filedec_t* filedec)
{
    if (filedec != NULL) {
        nucdec_free(filedec->nucdec);
        qualdec_free(filedec->qualdec);
        auxdec_free(filedec->auxdec);
        str_free(filedec->stats);
        free((void*)filedec);
        filedec = NULL;
    } else { /* filedec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

str_t* filedec_decode(filedec_t* filedec)
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

    if (fread_buf(filedec->ifp, prog_name, sizeof(prog_name)) != sizeof(prog_name))
        tsc_error("Failed to read program name!\n");
    if (fread_buf(filedec->ifp, version, sizeof(version)) != sizeof(version))
        tsc_error("Failed to read version!\n");
    if (strncmp((const char*)version, (const char*)tsc_version->s, sizeof(version)))
        tsc_error("File was compressed with another version: %s\n", version);
    if (fread_uint64(filedec->ifp, &block_sz) != sizeof(block_sz))
        tsc_error("Failed to read block size!\n");

    prog_name[3] = '\0';
    tsc_log("Format: %s %s\n", prog_name, version);
    tsc_log("Block size: %"PRIu64"\n", block_sz);

    /* Write SAM header:
     * - uint32_t       : header size
     * - unsigned char[]: data
     */
    if (tsc_verbose) tsc_log("Reading SAM header\n");
    uint32_t header_sz;
    if (fread_uint32(filedec->ifp, &header_sz) != sizeof(header_sz))
        tsc_error("Failed to read SAM header size!\n");
    str_t* header = str_new();
    str_reserve(header, header_sz);
    if (fread_buf(filedec->ifp, (unsigned char*)header->s, header_sz) != header_sz)
        tsc_error("Failed to read SAM header!\n");
    byte_cnt += fwrite_cstr(filedec->ofp, header->s);
    str_free(header);

    /* Read block header:
     * - uint32_t: block count
     * - uint32_t: no. of lines in block
     */
    uint32_t block_cnt = 0;
    uint32_t block_line_cnt = 0;

    /* Prepare decoding. */
    enum { QNAME, FLAG, RNAME, POS, MAPQ, CIGAR,
           RNEXT, PNEXT, TLEN, SEQ, QUAL, OPT };
    char* sam_fields[12];

    while (fread_uint32(filedec->ifp, &block_cnt) == sizeof(block_cnt)) {
        if (fread_uint32(filedec->ifp, &block_line_cnt) != sizeof(block_line_cnt))
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
        nucdec_decode_block(filedec->nucdec, filedec->ifp, pos, cigar, seq);
        qualdec_decode_block(filedec->qualdec, filedec->ifp, qual);
        auxdec_decode_block(filedec->auxdec, filedec->ifp, aux);

        /* Write NUC, QUAL and AUX in correct order to outfile. */
        if (tsc_verbose) tsc_log("Writing decoded block %"PRIu32": %zu lines\n", block_cnt, block_line_cnt);
        for (i = 0; i < block_line_cnt; i++) {
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
                byte_cnt += fwrite_cstr(filedec->ofp, sam_fields[f]);
                if (f != 11) byte_cnt += fwrite_byte(filedec->ofp, '\t');
            }
            byte_cnt += fwrite_byte(filedec->ofp, '\n');

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
    str_append_cstr(filedec->stats, stats);

    return filedec->stats;
}

