// Copyright 2015 Leibniz University Hannover (LUH)

#include "tsc.h"

#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "fio.h"
#include "log.h"
#include "samcodec.h"

// Options/flags from getopt
static const char *opt_input = NULL;
static const char *opt_output = NULL;
static const char *opt_blocksz = NULL;
static bool opt_flag_force = false;
static bool opt_flag_stats = false;

// Initializing global vars from 'tsclib.h'
str_t *tsc_prog_name = NULL;
str_t *tsc_version = NULL;
str_t *tsc_in_fname = NULL;
str_t *tsc_out_fname = NULL;
FILE *tsc_in_fp = NULL;
FILE *tsc_out_fp = NULL;
tsc_mode_t tsc_mode = TSC_MODE_COMPRESS;
bool tsc_stats = false;
unsigned int tsc_blocksz = 0;

static void print_help() {
    tsc_log("Usage: tsc [OPTIONS] FILE\n");
    tsc_log("TNT Sequence Compressor\n");
    tsc_log("\n");
    tsc_log("Options:\n");
    tsc_log("  -b, --blocksz=SIZE    specify block SIZE\n");
    tsc_log("  -d, --decompress      decompress\n");
    tsc_log("  -f, --force           force overwriting of output file(s)\n");
    tsc_log("  -h, --help            print this help\n");
    tsc_log("  -i, --info            print information about TSC file\n");
    tsc_log("  -o, --output=OUTFILE  specify output FILE\n");
    tsc_log("  -s, --stats           print (de-)compression statistics\n");
}

static void parse_options(int argc, char *argv[]) {
    int opt;

    static struct option long_options[] = {
        {"blocksz", required_argument, NULL, 'b'}, {"decompress", no_argument, NULL, 'd'},
        {"force", no_argument, NULL, 'f'},         {"help", no_argument, NULL, 'h'},
        {"info", no_argument, NULL, 'i'},          {"output", required_argument, NULL, 'o'},
        {"stats", no_argument, NULL, 's'},         {NULL, 0, NULL, 0}};

    const char *short_options = "b:dfhio:s";

    do {
        int opt_idx = 0;
        opt = getopt_long(argc, argv, short_options, long_options, &opt_idx);
        switch (opt) {
            case -1:
                break;
            case 'b':
                opt_blocksz = optarg;
                break;
            case 'd':
                if (tsc_mode == TSC_MODE_INFO) tsc_error("Cannot decompress and get info at once\n");
                tsc_mode = TSC_MODE_DECOMPRESS;
                break;
            case 'f':
                opt_flag_force = true;
                break;
            case 'h':
                print_help();
                exit(EXIT_SUCCESS);
            case 'i':
                if (tsc_mode == TSC_MODE_DECOMPRESS) tsc_error("Cannot decompress and get info at once\n");
                tsc_mode = TSC_MODE_INFO;
                break;
            case 'o':
                opt_output = optarg;
                break;
            case 's':
                opt_flag_stats = true;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    } while (opt != -1);

    // The input file must be the one remaining command line argument
    if (argc - optind > 1)
        tsc_error("Only one input file allowed\n");
    else if (argc - optind < 1)
        tsc_error("Input file missing\n");
    else
        opt_input = argv[optind];
}

static const char *fext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) {
        return "";
    }
    return (dot + 1);
}

static void handle_signal(int sig) {
    signal(sig, SIG_IGN);  // Ignore the signal
    tsc_log("Catched signal: %d\n", sig);
    signal(sig, SIG_DFL);  // Invoke default signal action
    raise(sig);
}

int main(int argc, char *argv[]) {
    tsc_prog_name = str_new();
    tsc_version = str_new();
    str_append_cstr(tsc_version, "");
    tsc_in_fname = str_new();
    tsc_out_fname = str_new();

    // Determine program name and truncate path if needed
    const char *prog_name = argv[0];
    char *p;
    if ((p = strrchr(argv[0], '/')) != NULL) prog_name = p + 1;
    str_copy_cstr(tsc_prog_name, prog_name);

    // If invoked as 'de...' switch to decompressor mode
    if (!strncmp(tsc_prog_name->s, "de", 2)) tsc_mode = TSC_MODE_DECOMPRESS;

    // Install signal handler for the following signal types:
    //   SIGTERM  termination request, sent to the program
    //   SIGSEGV  invalid memory access (segmentation fault)
    //   SIGINT   external interrupt, usually initiated by the user
    //   SIGILL   invalid program image, such as invalid instruction
    //   SIGABRT  abnormal termination condition, as is e.g. initiated by
    //            std::abort()
    //   SIGFPE   erroneous arithmetic operation such as divide by zero
    signal(SIGABRT, handle_signal);
    signal(SIGFPE, handle_signal);
    signal(SIGILL, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGSEGV, handle_signal);
    signal(SIGTERM, handle_signal);

    // Parse command line options and check them for sanity
    parse_options(argc, argv);
    str_copy_cstr(tsc_in_fname, opt_input);
    if (opt_flag_stats) tsc_stats = true;

    if (tsc_mode == TSC_MODE_COMPRESS) {
        // All possible options are legal in this mode
        if (!opt_blocksz) {
            tsc_blocksz = 10000;  // Default value
        } else {
            if (strtol(opt_blocksz, NULL, 10) <= 0) {
                tsc_error("Block size must be positive\n");
            } else {
                tsc_blocksz = (unsigned int)strtol(opt_blocksz, NULL, 10);
            }
        }
    } else if (tsc_mode == TSC_MODE_DECOMPRESS) {
        // Option -b is illegal in this mode
        if (opt_blocksz) {
            tsc_error("Illegal option(s) detected\n");
        }
    } else {  // TSC_MODE_INFO */
        // Options -bfos are illegal in this mode
        if (opt_blocksz || opt_flag_force || opt_output || opt_flag_stats) {
            tsc_error("Illegal option(s) detected\n");
        }
    }

    if (access((const char *)tsc_in_fname->s, F_OK | R_OK))  // NOLINT(hicpp-signed-bitwise)
        tsc_error("Cannot access input file: %s\n", tsc_in_fname->s);

    if (tsc_mode == TSC_MODE_COMPRESS) {
        // Check I/O
        if (strcmp(fext((const char *)tsc_in_fname->s), "sam") != 0) tsc_error("Input file extension must be 'sam'\n");

        if (opt_output == NULL) {
            str_copy_str(tsc_out_fname, tsc_in_fname);
            str_append_cstr(tsc_out_fname, ".tsc");
        } else {
            str_copy_cstr(tsc_out_fname, opt_output);
        }

        if (!access((const char *)tsc_out_fname->s, F_OK | W_OK) &&
            opt_flag_force == false) {  // NOLINT(hicpp-signed-bitwise)
            tsc_log("Output file already exists: %s\n", tsc_out_fname->s);
            tsc_log("Do you want to overwrite %s? ", tsc_out_fname->s);
            if (!yesno()) exit(EXIT_SUCCESS);
        }

        // Invoke compressor
        tsc_in_fp = tsc_fopen((const char *)tsc_in_fname->s, "r");
        tsc_out_fp = tsc_fopen((const char *)tsc_out_fname->s, "wb");
        tsc_log("Compressing: %s\n", tsc_in_fname->s);
        samcodec_t *samcodec = samcodec_new(tsc_in_fp, tsc_out_fp, tsc_blocksz);
        samcodec_encode(samcodec);
        samcodec_free(samcodec);
        tsc_log("Finished: %s\n", tsc_out_fname->s);
        tsc_fclose(tsc_in_fp);
        tsc_fclose(tsc_out_fp);
    } else if (tsc_mode == TSC_MODE_DECOMPRESS) {
        // Check I/O
        if (strcmp(fext((const char *)tsc_in_fname->s), "tsc") != 0) tsc_error("Input file extension must be 'tsc'\n");

        if (opt_output == NULL) {
            str_copy_str(tsc_out_fname, tsc_in_fname);
            str_trunc(tsc_out_fname, 4);  // strip '.tsc'
            if (strcmp(fext((const char *)tsc_out_fname->s), "sam") != 0) str_append_cstr(tsc_out_fname, ".sam");
        } else {
            str_copy_cstr(tsc_out_fname, opt_output);
        }

        if (!access((const char *)tsc_out_fname->s, F_OK | W_OK) &&
            opt_flag_force == false) {  // NOLINT(hicpp-signed-bitwise)
            tsc_log("Output file already exists: %s\n", tsc_out_fname->s);
            tsc_log("Do you want to overwrite %s? ", tsc_out_fname->s);
            if (!yesno()) exit(EXIT_SUCCESS);
        }

        // Invoke decompressor
        tsc_in_fp = tsc_fopen((const char *)tsc_in_fname->s, "rb");
        tsc_out_fp = tsc_fopen((const char *)tsc_out_fname->s, "w");
        tsc_log("Decompressing: %s\n", tsc_in_fname->s);
        samcodec_t *samcodec = samcodec_new(tsc_in_fp, tsc_out_fp, 0);
        samcodec_decode(samcodec);
        samcodec_free(samcodec);
        tsc_log("Finished: %s\n", tsc_out_fname->s);
        tsc_fclose(tsc_in_fp);
        tsc_fclose(tsc_out_fp);
    } else {  // TSC_MODE_INFO
        // Check I/O
        if (strcmp(fext((const char *)tsc_in_fname->s), "tsc") != 0) tsc_error("Input file extension must be 'tsc'\n");

        // Read information from compressed tsc file
        tsc_in_fp = tsc_fopen((const char *)tsc_in_fname->s, "rb");
        tsc_log("Reading information: %s\n", tsc_in_fname->s);
        samcodec_t *samcodec = samcodec_new(tsc_in_fp, NULL, 0);
        samcodec_info(samcodec);
        samcodec_free(samcodec);
        tsc_fclose(tsc_in_fp);
    }

    str_free(tsc_prog_name);
    str_free(tsc_version);
    str_free(tsc_in_fname);
    str_free(tsc_out_fname);

    return EXIT_SUCCESS;
}
