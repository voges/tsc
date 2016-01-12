/*
 * The copyright in this software is being made available under the TNT
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2015, Leibniz Universitaet Hannover, Institut fuer
 * Informationsverarbeitung (TNT)
 * Contact: <voges@tnt.uni-hannover.de>
 * All rights reserved.
 *
 * * Redistribution in source or binary form is not permitted.
 *
 * * Use in source or binary form is only permitted in the context of scientific
 *   research.
 *
 * * Commercial use without specific prior written permission is prohibited.
 *   Neither the name of the TNT nor the names of its contributors may be used
 *   to endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _GNU_SOURCE

#include "tsclib.h"
#include "tnt.h"
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

static bool tsc_yesno(void)
{
    int c = getchar();
    bool yes = c == 'y' || c == 'Y';
    while (c != '\n' && c != EOF)
        c = getchar();
    return yes;
}

void tsc_cleanup(void)
{
    if (tsc_in_fp != NULL)
        tnt_fclose(tsc_in_fp);
    if (tsc_out_fp != NULL)
        tnt_fclose(tsc_out_fp);
    if (tsc_out_fname->len > 0) {
        tsc_log("Do you want to remove %s (y/n)? ", tsc_out_fname->s);
        if (tsc_yesno()) {
            unlink((const char *)tsc_out_fname->s);
            tsc_log("Removed %s\n", tsc_out_fname->s);
        }
    }
}

void tsc_abort(void)
{
    tsc_cleanup();
    exit(EXIT_FAILURE);
}

void tsc_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stdout, "%s", msg);
    free(msg);
}

void tsc_error(const char *fmt, ...)
{
    tsc_cleanup();

    va_list args;
    va_start(args, fmt);
    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);
    fprintf(stdout, "Error: %s", msg);
    free(msg);
}

