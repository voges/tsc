/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <voges@tnt.uni-hannover.de>
 */

/*
 * This file is part of gomp.
 *
 * Gomp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gomp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gomp. If not, see <http: *www.gnu.org/licenses/>
 */

#include "gomplib.h"
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

/* Initializing global vars from 'gomplib.h' */
str_t* gomp_prog_name = NULL;
str_t* gomp_version = NULL;
str_t* gomp_in_fname = NULL;
str_t* gomp_out_fname = NULL;
FILE* gomp_in_fp = NULL;
FILE* gomp_out_fp = NULL;
gomp_mode_t gomp_mode = gomp_MODE_COMPRESS;
bool gomp_stats = false;
bool gomp_time = false;
bool gomp_verbose = false;

static void print_version(void)
{
    printf("%s %s (framework)\n", gomp_prog_name->s, gomp_version->s);
}

static void print_copyright(void)
{
    printf("Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)\n");
    printf("Contact: Jan Voges <voges@tnt.uni-hannover.de>\n");
}

static void print_help(void)
{
    print_version();
    print_copyright();
    printf("\n");
    printf("Usage:\n");
    printf("  Compress  : gomp [-o output] [-fstv] <file.sam>\n");
    printf("  Decompress: gomp -d [-o output] [-fstv] <file.gomp>\n");
    printf("              degomp [-o output] [-fstv] <file.gomp>\n");
    printf("  Info      : gomp -i [-v] <file.gomp>\n");
    printf("              degomp -i [-v] <file.gomp>\n");
    printf("\n");
    printf("Options:\n");
    printf("  -d  --decompress  Decompress\n");
    printf("  -f, --force       Force overwriting of output file(s)\n");
    printf("  -h, --help        Print this help\n");
    printf("  -i, --info        Print information about gomp file\n");
    printf("  -o, --output      Specify output file\n");
    printf("  -s, --stats       Print (de-)compression statistics\n");
    printf("  -t, --time        Print (de-)compression timing statistics\n");
    printf("  -v, --verbose     Verbose output\n");
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
            gomp_mode = gomp_MODE_DECOMPRESS;
            break;
        case 'f':
            opt_flag_force = true;
            break;
        case 'h':
            print_help();
            exit(EXIT_SUCCESS);
            break;
        case 'i':
            gomp_mode = gomp_MODE_INFO;
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
        gomp_error("Only one input file allowed\n");
    else if (argc - optind < 1)
        gomp_error("Input file missing\n");
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
    gomp_log("Catched signal: %d\n", sig);
    gomp_log("Cleaning up ...\n");
    gomp_cleanup();
    signal(sig, SIG_DFL); /* invoke default signal action */
    raise(sig);
}

int main(int argc, char* argv[])
{
    gomp_prog_name = str_new();
    gomp_version = str_new();
    str_copy_cstr(gomp_version, VERSION);
    gomp_in_fname = str_new();
    gomp_out_fname = str_new();

    /* Determine program name. Truncate path if needed. */
    const char* prog_name = argv[0];
    char* p;
    if ((p = strrchr(argv[0], '/')) != NULL)
        prog_name = p + 1;
    str_copy_cstr(gomp_prog_name, prog_name);

    /* If invoked as 'de...', switch to decompressor mode. */
    if (!strncmp(gomp_prog_name->s, "de", 2)) gomp_mode = GOMP_MODE_DECOMPRESS;

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
    str_copy_cstr(gomp_in_fname, opt_input);
    if (opt_flag_stats) gomp_stats = true;
    if (opt_flag_time) gomp_time = true;
    if (opt_flag_verbose) gomp_verbose = true;

    if (gomp_mode == GOMP_MODE_COMPRESS) {
        /* All possible options are legal in this mode. */
    } else if (gomp_mode == GOMP_MODE_DECOMPRESS) {
        /* All possible options are legal in this mode. */
    } else { /* GOMP_MODE_INFO */
        /* Options -fost are illegal in this mode. */
        if (opt_flag_force || opt_output || opt_flag_stats || opt_flag_time)
            gomp_error("Illegal options detected\n");
    }

    /* Check if input file is accessible. */
    if (access((const char*)gomp_in_fname->s, F_OK | R_OK))
        gomp_error("Cannot access input file: %s\n", gomp_in_fname->s);

    if (gomp_mode == GOMP_MODE_COMPRESS) {
        if (strcmp(file_extension((const char*)gomp_in_fname->s), "sam")) {
            gomp_error("Input file has wrong extension (must be .sam): %s\n",
                       gomp_in_fname->s);
        }

        if (opt_output == NULL) {
            str_copy_str(gomp_out_fname, gomp_in_fname);
            str_append_cstr(gomp_out_fname, ".gomp");
        } else {
            str_copy_cstr(gomp_out_fname, opt_output);
        }

        if (!access((const char*)gomp_out_fname->s, F_OK | W_OK)
            && opt_flag_force == false) {
            gomp_warning("Output file already exists (use '-f' to force "
                         "overwriting): %s\n", gomp_out_fname->s);
            exit(EXIT_FAILURE);
        }

        /* Invoke compressor */
        gomp_in_fp = gomp_fopen((const char*)gomp_in_fname->s, "r");
        gomp_out_fp = gomp_fopen((const char*)gomp_out_fname->s, "wb");
        gomp_log("Compressing: %s\n", gomp_in_fname->s);
        fileenc_t* fileenc = fileenc_new(gomp_in_fp, gomp_out_fp);
        fileenc_encode(fileenc);
        fileenc_free(fileenc);
        gomp_log("Finished: %s\n", gomp_out_fname->s);
        gomp_fclose(gomp_in_fp);
        gomp_fclose(gomp_out_fp);
    } else  if (gomp_mode == GOMP_MODE_DECOMPRESS) {
        if (strcmp(file_extension((const char*)gomp_in_fname->s), "gomp")) {
            gomp_error("Input file has wrong extension (must be .gomp): %s\n",
                       gomp_in_fname->s);
        }

        if (opt_output == NULL) {
            str_copy_str(gomp_out_fname, gomp_in_fname);
            str_trunc(gomp_out_fname, 4); /* strip '.gomp' */
            if (strcmp(file_extension((const char*)gomp_out_fname->s), "sam"))
                str_append_cstr(gomp_out_fname, ".sam");
        } else {
            str_copy_cstr(gomp_out_fname, opt_output);
        }

        if (!access((const char*)gomp_out_fname->s, F_OK | W_OK)
                && opt_flag_force == false) {
            gomp_warning("Output file already exists (use '-f' to force "
                        "overwriting): %s\n", gomp_out_fname->s);
            exit(EXIT_FAILURE);
        }

        /* Invoke decompressor */
        gomp_in_fp = gomp_fopen((const char*)gomp_in_fname->s, "rb");
        gomp_out_fp = gomp_fopen((const char*)gomp_out_fname->s, "w");
        gomp_log("Decompressing: %s\n", gomp_in_fname->s);
        filedec_t* filedec = filedec_new(gomp_in_fp, gomp_out_fp);
        filedec_decode(filedec);
        filedec_free(filedec);
        gomp_log("Finished: %s\n", gomp_out_fname->s);
        gomp_fclose(gomp_in_fp);
        gomp_fclose(gomp_out_fp);
    } else { /* GOMP_MODE_INFO */
        if (strcmp(file_extension((const char*)gomp_in_fname->s), "gomp")) {
            gomp_error("Input file has wrong extension (must be .gomp): %s\n",
                       gomp_in_fname->s);
        }

        /* Generate information about compressed gomp file. */
        gomp_in_fp = gomp_fopen((const char*)gomp_in_fname->s, "rb");
        gomp_log("Reading information: %s\n", gomp_in_fname->s);
        filedec_t* filedec = filedec_new(gomp_in_fp, NULL);
        filedec_info(filedec);
        filedec_free(filedec);
        gomp_fclose(gomp_in_fp);
    }

    str_free(gomp_prog_name);
    str_free(gomp_version);
    str_free(gomp_in_fname);
    str_free(gomp_out_fname);

    return EXIT_SUCCESS;
}

