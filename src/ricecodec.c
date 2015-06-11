/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "ricecodec.h"

unsigned char *rice_compress(unsigned char* in,
                             unsigned int   in_size,
                             unsigned int*  out_size)
{

}

unsigned char *rice_uncompress(unsigned char* in,
                               unsigned int   in_size,
                               unsigned int*  out_size)
{

}



/* Rice [en]coder
 * Demonstration code by Emil Mikulic, 2002.
 * http://purl.org/net/overload
 *
 * $Id: encode.c,v 1.2 2002/12/10 14:56:55 emikulic Exp $
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

FILE *fp;
unsigned char buff = 0;
int filled = 0;

void put_bit(unsigned char b)
{
    buff = buff | ((b & 1) << filled);
    if (filled == 7)
    {
        if (!fwrite(&buff, 1, 1, fp))
        {
            printf("\nerror writing to file!\n");
            exit(1);
        }

        buff = 0;
        filled = 0;
    }
    else
        filled++;
}



void rice_code(unsigned char x, int k)
{
    int m = 1 << k;
    int q = x / m;
    int i;

    for (i=0; i<q; i++) put_bit(1);
    put_bit(0);

    for (i=k-1; i>=0; i--) put_bit( (x >> i) & 1 );
}



int rice_len(unsigned char x, int k)
{
    int m = 1 << k;
    int q = x / m;
    return q + 1 + k;
}



int main(int argv, char **argc)
{
    unsigned int fs, i, k, outsize, best_size = 0, best_k;
    unsigned char *buf;
    struct stat stat;

    if (argv != 3)
    {
        printf("usage: %s <infile> <outfile>\n", argc[0]);
        return 1;
    }

    fp = fopen(argc[1], "rb");
    if (fp)
        printf("reading from %s ", argc[1]);
    else {
        printf("error opening %s!\n", argc[1]);
        return 1;
    }

    fstat(fileno(fp), &stat);
    fs = (unsigned int)stat.st_size;
    printf("(%d bits)\n", fs*8);

    buf = (unsigned char*) malloc(fs);
    if (!buf)
    {
        printf("can't allocate memory!\n");
        return 1;
    }

    if (!fread(buf, fs, 1, fp))
    {
        printf("error reading from file!\n");
        return 1;
    }
    fclose(fp);

    /* find length */
    for (k=0; k<8; k++)
    {
        printf("k = %d, ", k);
        fflush(stdout);

        outsize = 0;
        for (i=0; i<fs; i++)
            outsize += rice_len(buf[i], k);

        printf("size = %d bits\n", outsize);

        if (!best_size || outsize < best_size)
        {
            best_size = outsize;
            best_k = k;
        }
    }

    /* output */
    fp = fopen(argc[2], "wb");
    if (fp)
    {
        printf("writing to %s...", argc[2]);
        fflush(stdout);
    } else {
        printf("error opening %s!\n", argc[2]);
        return 1;
    }

    printf("(k = %d)...", best_k);
    put_bit((best_k >> 2) & 1);
    put_bit((best_k >> 1) & 1);
    put_bit((best_k     ) & 1);

    for (i=0; i<fs; i++)
        rice_code(buf[i], best_k);

    /* flush */
    i = 8 - filled;
    while (i--) put_bit(1);

    fclose(fp);
    free(buf);
    printf("done!\n");
    return 0;
}

/* Rice [de]coder
 * Demonstration code by Emil Mikulic, 2002.
 * http://purl.org/net/overload
 *
 * $Id: decode.c,v 1.2 2002/12/10 14:56:22 emikulic Exp $
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

FILE *fin, *fout;
unsigned char buff;
int done = 0, filled = 0;

unsigned char get_bit(void)
{
    unsigned char tmp;

    if (!filled)
    {
        if (!fread(&buff, 1, 1, fin))
        {
            done = 1;
            return 0;
        }
        filled = 8;
    }

    tmp = buff & 1;
    buff = buff >> 1;
    filled--;

    return tmp;
}



int main(int argv, char **argc)
{
    unsigned int k;
    unsigned char *buf;

    if (argv != 3)
    {
        printf("usage: %s <infile> <outfile>\n", argc[0]);
        return 1;
    }

    fin = fopen(argc[1], "rb");
    if (fin)
        printf("reading from %s... ", argc[1]);
    else {
        printf("error opening %s!\n", argc[1]);
        return 1;
    }

    k = (get_bit() << 2) | (get_bit() << 1) | get_bit();
    if (done)
    {
        printf("couldn't read three bits!\n");
        return 1;
    }
    printf("k = %d\n", k);

    fout = fopen(argc[2], "wb");
    if (fout)
    {
        printf("writing to %s\n", argc[2]);
        fflush(stdout);
    } else {
        printf("error opening %s!\n", argc[2]);
        return 1;
    }

    while (1)
    {
        int m = 1 << k, q = 0, x, i;

        while (get_bit()) q++;
        x = m * q;

        for (i=k-1; i>=0; i--) x = x | (get_bit() << i);

        if (done) break;
        if (!fwrite(&x, 1, 1, fout))
        {
            printf("error writing to file!\n");
            return 1;
        }
    }

    fclose(fin);
    fclose(fout);
    printf("done!\n");
    return 0;
}
