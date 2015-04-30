/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include "tsclib.h"
#include "version.h"
#include "fileenc.h"

/* Options/flags from getopt */
static const char* opt_input = NULL;
static const char* opt_output = NULL;
static bool opt_flag_force = false;

/* Initializing global vars from 'tsclib.h' */
str_t* tsc_prog_name = NULL;
str_t* tsc_in_fname = NULL;
FILE* tsc_in_fd = NULL;
str_t* tsc_out_fname = NULL;
FILE* tsc_out_fd = NULL;
tsc_mode_t tsc_mode = TSC_MODE_COMPRESS;

static void print_version(void)
{
    printf("%s %s\n", tsc_prog_name->s, VERSION);
}

static void print_copyright(void)
{
    printf("Copyright (c) 2015 by Jan Voges <jvoges@tnt.uni-hannover.de>\n");
}

static void print_help(void)
{
    print_version();
    print_copyright();
    printf("\n");
    printf("Usage: Compress:   tsc <file.sam>\n");
    printf("       Decompress: tsc -d <file.sam.ts>\n\n");
    printf("Options:\n");
    printf("  -h, --help       Print this help\n");
    printf("  -d  --decompress Decompress\n");
    printf("  -o, --output     Specify output file\n");
    printf("  -f  --force      Force overwriting of output file\n");
    printf("  -v  --version    Display program version\n");
    printf("\n");
}

static void parse_options(int argc, char *argv[])
{
    int opt;

    static struct option long_options[] = {
        { "help",       no_argument,       NULL, 'h'},
        { "decompress", no_argument,       NULL, 'd'},
        { "output",     required_argument, NULL, 'o'},
        { "force",      no_argument,       NULL, 'f'},
        { "version",    no_argument,       NULL, 'v'},
        { NULL,         0,                 NULL,  0 }
    };

    const char *short_options = "hdo:fv";

    do {
        int opt_idx = 0;
        opt = getopt_long(argc, argv, short_options, long_options, &opt_idx);
        switch (opt) {
            case -1:
                break;
            case 'h':
                print_help();
                exit(EXIT_SUCCESS);
                break;
            case 'd':
                tsc_mode = TSC_MODE_DECOMPRESS;
                break;
            case 'o':
                opt_output = optarg;
                break;
            case 'f':
                opt_flag_force = true;
                break;
            case 'v':
                print_version();
                exit(EXIT_SUCCESS);
                break;
            default:
                exit(EXIT_FAILURE);
        }
    } while (opt != -1);

    /* There must be exactly one remaining CL argument (the input file). */
    if (argc - optind > 1) {
        tsc_error("Too many input files.");
    }
    else if (argc - optind < 1) {
        tsc_error("Input file missing.");
    }
    else {
        opt_input = argv[optind];
    }
}

static const char* file_extension(const char* filename)
{
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) { return ""; }
    return (dot + 1);
}

static void handle_signal(int sig)
{
    signal(sig, SIG_IGN); /* Ignore the signal */
    tsc_log("\nCatched signal: %d", sig);
    tsc_log("Cleaning up ...");
    tsc_cleanup();
    signal(sig, SIG_DFL); /* Invoke default signal action */
    raise(sig);
}

int main(int argc, char *argv[])
{
    /* Initialize strings */
    tsc_prog_name = str_new();
    tsc_in_fname = str_new();
    tsc_out_fname = str_new();

    /* Determine program name */
    const char* prog_name = argv[0];
    char* p;
    if ((p = strrchr(argv[0], '/')) != NULL) { prog_name = p + 1; }
    str_copy_cstr(tsc_prog_name, prog_name, strlen(prog_name));

    /* Invoke own signal handler */
    signal(SIGHUP,  handle_signal);
    signal(SIGQUIT, handle_signal);
    signal(SIGABRT, handle_signal);
    signal(SIGPIPE, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGXCPU, handle_signal);
    signal(SIGXFSZ, handle_signal);

    /* Parse command line options */
    parse_options(argc, argv);

    /* Input file name */
    str_copy_cstr(tsc_in_fname, opt_input, strlen(opt_input));

    /* Check if input file is accessible */
    if (access((const char*)tsc_in_fname->s, F_OK | R_OK)) {
        tsc_error("Cannot access input file: %s", tsc_in_fname->s);
    }

    if (tsc_mode == TSC_MODE_COMPRESS) {

        /* Check input file extension */
        if (strcmp(file_extension((const char*)tsc_in_fname->s), "sam")) {
            tsc_error("Input file has wrong extension (must be .sam): %s", tsc_in_fname->s);
        }

        /* If output file name is not provided, make a reasonable name for it. */
        if (opt_output == NULL) {
            str_copy_str(tsc_out_fname, tsc_in_fname);
            str_append_cstr(tsc_out_fname, ".tsc");

        } else {
            str_copy_cstr(tsc_out_fname, opt_output, strlen(opt_output));
        }

        /* Check if output file already exists */
        if (!access((const char*)tsc_out_fname->s, F_OK | W_OK) && opt_flag_force == false) {
            tsc_warning("Output file already exists (use '-f' to force overwriting): %s", tsc_out_fname->s);
            exit(EXIT_FAILURE);
        }

        /* Open infile, create and open outfile */
        tsc_in_fd = tsc_fopen_or_die((const char*)tsc_in_fname->s, "r");
        tsc_out_fd = tsc_fopen_or_die((const char*)tsc_out_fname->s, "wb");

        /* Do it! :) */
        tsc_log("Encoding %s -> %s ...", tsc_in_fname->s, tsc_out_fname->s);
        fileenc_t* fileenc = fileenc_new(tsc_in_fd, tsc_out_fd);
        fileenc_encode(fileenc);
        fileenc_free(fileenc);

        /* Close files */
        tsc_fclose_or_die(tsc_in_fd);
        tsc_fclose_or_die(tsc_out_fd);
    } else { /* TSC_MODE_DECOMPRESS */

        /* Check input file extension */
        if (strcmp(file_extension((const char*)tsc_in_fname->s), "tsc")) {
            tsc_error("Input file has wrong extension (must be .tsc): %s", tsc_in_fname->s);
        }

        /* If output file name is not provided, make a reasonable name for it. */
        if (opt_output == NULL) {
            str_copy_str(tsc_out_fname, tsc_in_fname);
            str_trunc(tsc_out_fname, 4); /* strip '.tsc' */

        } else {
            str_copy_cstr(tsc_out_fname, opt_output, strlen(opt_output));
        }

        /* Check if output file already exists */
        if (!access((const char*)tsc_out_fname->s, F_OK | W_OK) && opt_flag_force == false) {
            tsc_warning("Output file already exists (use '-f' to force overwriting): %s", tsc_out_fname->s);
            exit(EXIT_FAILURE);
        }

        /* Open infile, create and open outfile */
        tsc_in_fd = tsc_fopen_or_die((const char*)tsc_in_fname->s, "rb");
        tsc_out_fd = tsc_fopen_or_die((const char*)tsc_out_fname->s, "w");

        /* Do it! :) */
        tsc_log("Decoding %s -> %s ...", tsc_in_fname->s, tsc_out_fname->s);

        /* Close files */
        tsc_fclose_or_die(tsc_in_fd);
        tsc_fclose_or_die(tsc_out_fd);
    }

    str_free(tsc_prog_name);
    str_free(tsc_in_fname);
    str_free(tsc_out_fname);

    return EXIT_SUCCESS;
}
