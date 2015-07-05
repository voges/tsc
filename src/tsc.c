/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "tsclib.h"
#include "filecodec.h"
#include "version.h"
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Options/flags from getopt */
static const char* opt_input = NULL;
static const char* opt_output = NULL;
static bool opt_flag_force = false;
static bool opt_flag_stats = false;
static bool opt_flag_time = false;
static bool opt_flag_verbose = false;

/* Initializing global vars from 'tsclib.h' */
str_t* tsc_prog_name = NULL;
str_t* tsc_version = NULL;
str_t* tsc_in_fname = NULL;
str_t* tsc_out_fname = NULL;
FILE* tsc_in_fp = NULL;
FILE* tsc_out_fp = NULL;
tsc_mode_t tsc_mode = TSC_MODE_COMPRESS;
bool tsc_stats = false;
bool tsc_time = false;
bool tsc_verbose = false;

static void print_version(void)
{
    printf("%s %s\n", tsc_prog_name->s, tsc_version->s);
}

static void print_copyright(void)
{
    printf("Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)\n");
    printf("Contact: Jan Voges <jvoges@tnt.uni-hannover.de>\n");
}

static void print_help(void)
{
    print_version();
    print_copyright();
    printf("\n");
    printf("Usage:\n");
    printf("  Compress   : tsc [-o output] [-fstv] <file.sam>\n");
    printf("  Decompress : tsc -d [-o output] [-fstv] <file.tsc>\n");
    printf("               detsc [-o output] [-fstv] <file.tsc>\n");
    printf("  Info       : tsc -i [-v] <file.tsc>\n");
    printf("               detsc -i [-v] <file.tsc>\n");
    printf("\n");
    printf("Examples (using default preferences):\n");
    printf("  Compress   : tsc test.sam\n");
    printf("  Decompress : detsc test.sam.tsc\n");
    printf("  Info       : detsc -i test.sam.tsc\n");
    printf("\n");
    printf("Options:\n");
    printf("  -d  --decompress  Decompress\n");
    printf("  -f, --force       Force overwriting of output file(s)\n");
    printf("  -h, --help        Print this help\n");
    printf("  -i, --info        Print information about tsc file\n");
    printf("  -o, --output      Specify output file\n");
    printf("  -s, --stats       Print (de-)compression statistics\n");
    printf("  -t, --time        Print timing statistics\n");
    printf("  -v, --verbose     Print detailed information\n");
    printf("  -V, --version     Display program version\n");
    printf("\n");
}

static void parse_options(int argc, char *argv[])
{
    int opt;

    static struct option long_options[] = {
        { "decompress", no_argument,       NULL, 'd'},
        { "force",      no_argument,       NULL, 'f'},
        { "help",       no_argument,       NULL, 'h'},
        { "info",       no_argument,       NULL, 'i'},
        { "output",     required_argument, NULL, 'o'},
        { "stats",      no_argument,       NULL, 's'},
        { "time",       no_argument,       NULL, 't'},
        { "verbose",    no_argument,       NULL, 'v'},
        { "version",    no_argument,       NULL, 'V'},
        { NULL,         0,                 NULL,  0 }
    };

    const char *short_options = "dfhio:stvV";

    do {
        int opt_idx = 0;
        opt = getopt_long(argc, argv, short_options, long_options, &opt_idx);
        switch (opt) {
        case -1:
            break;
        case 'd':
            tsc_mode = TSC_MODE_DECOMPRESS;
            break;
        case 'f':
            opt_flag_force = true;
            break;
        case 'h':
            print_help();
            exit(EXIT_SUCCESS);
            break;
        case 'i':
            tsc_mode = TSC_MODE_INFO;
            break;
        case 'o':
            opt_output = optarg;
            break;
        case 's':
            opt_flag_stats = true;
            break;
        case 't':
            opt_flag_time = true;
            break;
        case 'v':
            opt_flag_verbose = true;
            break;
        case 'V':
            print_version();
            exit(EXIT_SUCCESS);
            break;
        default:
            exit(EXIT_FAILURE);
        }
    } while (opt != -1);

    /* There must be exactly one remaining command line argument (the input
     * file).
     */
    if (argc - optind > 1)
        tsc_error("Only one input file allowed\n");
    else if (argc - optind < 1)
        tsc_error("Input file missing\n");
    else
        opt_input = argv[optind];
}

static const char* file_extension(const char* path)
{
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) { return ""; }
    return (dot + 1);
}

static void handle_signal(int sig)
{
    signal(sig, SIG_IGN); /* ignore the signal */
    tsc_log("Catched signal: %d\n", sig);
    tsc_log("Cleaning up ...\n");
    tsc_cleanup();
    signal(sig, SIG_DFL); /* invoke default signal action */
    raise(sig);
}

int main(int argc, char* argv[])
{
    tsc_prog_name = str_new();
    tsc_version = str_new();
    str_copy_cstr(tsc_version, VERSION);
    tsc_in_fname = str_new();
    tsc_out_fname = str_new();

    /* Determine program name. Truncate path if needed. */
    const char* prog_name = argv[0];
    char* p;
    if ((p = strrchr(argv[0], '/')) != NULL)
        prog_name = p + 1;
    str_copy_cstr(tsc_prog_name, prog_name);

    /* If invoked as 'de...', switch to decompressor mode. */
    if (!strncmp(tsc_prog_name->s, "de", 2)) tsc_mode = TSC_MODE_DECOMPRESS;

    /* Invoke custom signal handler */
    signal(SIGHUP,  handle_signal);
    signal(SIGQUIT, handle_signal);
    signal(SIGABRT, handle_signal);
    signal(SIGPIPE, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGXCPU, handle_signal);
    signal(SIGXFSZ, handle_signal);

    /* Parse command line options and check them for sanity. */
    parse_options(argc, argv);
    str_copy_cstr(tsc_in_fname, opt_input);
    if (opt_flag_stats) tsc_stats = true;
    if (opt_flag_time) tsc_time = true;
    if (opt_flag_verbose) tsc_verbose = true;

    if (tsc_mode == TSC_MODE_COMPRESS) {
        /* All possible options are legal in this mode. */
    } else if (tsc_mode == TSC_MODE_DECOMPRESS) {
        /* All possible options are legal in this mode. */
    } else { /* TSC_MODE_INFO */
        /* Options -fost are illegal in this mode. */
        if (opt_flag_force || opt_output || opt_flag_stats || opt_flag_time)
            tsc_error("Illegal options detected\n");
    }

    /* Check, if input file is accessible. */
    if (access((const char*)tsc_in_fname->s, F_OK | R_OK))
        tsc_error("Cannot access input file: %s\n", tsc_in_fname->s);

    if (tsc_mode == TSC_MODE_COMPRESS) {
        /* Check input file extension. */
        if (strcmp(file_extension((const char*)tsc_in_fname->s), "sam"))
            tsc_error("Input file has wrong extension (must be .sam): %s\n",
                      tsc_in_fname->s);

        /* If output file name is not provided, make a reasonable name for
         * it.
         */
        if (opt_output == NULL) {
            str_copy_str(tsc_out_fname, tsc_in_fname);
            str_append_cstr(tsc_out_fname, ".tsc");
        } else {
            str_copy_cstr(tsc_out_fname, opt_output);
        }

        /* Check if output file already exists. */
        if (!access((const char*)tsc_out_fname->s, F_OK | W_OK)
            && opt_flag_force == false) {
            tsc_warning("Output file already exists (use '-f' to force "
                        "overwriting): %s\n", tsc_out_fname->s);
            exit(EXIT_FAILURE);
        }

        /* Invoke compressor */
        tsc_in_fp = tsc_fopen((const char*)tsc_in_fname->s, "r");
        tsc_out_fp = tsc_fopen((const char*)tsc_out_fname->s, "wb");
        tsc_log("Compressing: %s\n", tsc_in_fname->s);
        fileenc_t* fileenc = fileenc_new(tsc_in_fp, tsc_out_fp);
        fileenc_encode(fileenc);
        fileenc_free(fileenc);
        tsc_log("Finished: %s\n", tsc_out_fname->s);
        tsc_fclose(tsc_in_fp);
        tsc_fclose(tsc_out_fp);
    } else  if (tsc_mode == TSC_MODE_DECOMPRESS) {
        /* Check input file extension. */
        if (strcmp(file_extension((const char*)tsc_in_fname->s), "tsc"))
            tsc_error("Input file has wrong extension (must be .tsc): %s\n",
                      tsc_in_fname->s);

        /* If output file name is not provided, make a reasonable name for
         * it.
         */
        if (opt_output == NULL) {
            str_copy_str(tsc_out_fname, tsc_in_fname);
            str_trunc(tsc_out_fname, 4); /* strip '.tsc' */
            if (strcmp(file_extension((const char*)tsc_out_fname->s), "sam"))
                str_append_cstr(tsc_out_fname, ".sam");
        } else {
            str_copy_cstr(tsc_out_fname, opt_output);
        }

        /* Check if output file already exists. */
        if (!access((const char*)tsc_out_fname->s, F_OK | W_OK)
                && opt_flag_force == false) {
            tsc_warning("Output file already exists (use '-f' to force "
                        "overwriting): %s\n", tsc_out_fname->s);
            exit(EXIT_FAILURE);
        }

        /* Invoke decompressor */
        tsc_in_fp = tsc_fopen((const char*)tsc_in_fname->s, "rb");
        tsc_out_fp = tsc_fopen((const char*)tsc_out_fname->s, "w");
        tsc_log("Decompressing: %s\n", tsc_in_fname->s);
        filedec_t* filedec = filedec_new(tsc_in_fp, tsc_out_fp);
        filedec_decode(filedec);
        filedec_free(filedec);
        tsc_log("Finished: %s\n", tsc_out_fname->s);
        tsc_fclose(tsc_in_fp);
        tsc_fclose(tsc_out_fp);
    } else { /* TSC_MODE_INFO */
        /* Check input file extension. */
        if (strcmp(file_extension((const char*)tsc_in_fname->s), "tsc"))
            tsc_error("Input file has wrong extension (must be .tsc): %s\n",
                      tsc_in_fname->s);

        /* Generate information about compressed tsc file. */
        tsc_in_fp = tsc_fopen((const char*)tsc_in_fname->s, "rb");
        tsc_log("Reading information: %s\n", tsc_in_fname->s);
        filedec_t* filedec = filedec_new(tsc_in_fp, NULL);
        filedec_info(filedec);
        filedec_free(filedec);
        tsc_fclose(tsc_in_fp);
    }

    str_free(tsc_prog_name);
    str_free(tsc_version);
    str_free(tsc_in_fname);
    str_free(tsc_out_fname);

    return EXIT_SUCCESS;
}

