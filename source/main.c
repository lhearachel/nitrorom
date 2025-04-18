// SPDX-License-Identifier: MIT

/*
 * nitrorom-pack - Produce a Nintendo DS ROM from sources
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "constants.h"
#include "packer.h"

#include "libs/clip.h"
#include "libs/config.h"
#include "libs/fileio.h"
#include "libs/sheets.h"
#include "libs/strings.h"

#define die(__msg, ...)                        \
    {                                          \
        fputs("nitrorom: ", stderr);           \
        fprintf(stderr, __msg, ##__VA_ARGS__); \
        fputc('\n', stderr);                   \
        exit(EXIT_FAILURE);                    \
    }

#define dieusage(__msg, ...)                 \
    {                                        \
        fputs("nitrorom: ", stderr);         \
        fprintf(stderr, __msg, __VA_ARGS__); \
        fputs("\n\n", stderr);               \
        showusage(stderr);                   \
        exit(EXIT_FAILURE);                  \
    }

#define dieiferr(__cond, __resT)                \
    {                                           \
        __resT __res = __cond;                  \
        if (__res.code != 0) {                  \
            fprintf(stderr, "%s\n", __res.msg); \
            exit(EXIT_FAILURE);                 \
        }                                       \
    }

typedef struct args {
    const char *config;
    const char *files;
    const char *workdir;
    const char *outfile;

    long dryrun;
    long verbose;
} args;

// clang-format off
static const cfgsection cfgsections[] = {
    { .section = string("header"),   .handler = cfg_header },
    { .section = string("rom"),      .handler = cfg_rom    },
    { .section = string("banner"),   .handler = cfg_banner },
    { .section = string("arm9"),     .handler = cfg_arm9   },
    { .section = string("arm7"),     .handler = cfg_arm7   },
    { .section = stringZ,            .handler = NULL       },
};
// clang-format on

static void   showusage(FILE *stream);
static args   parseargs(const char **argv);
static string tryfload(const char *filename);

#define dumpargs(__memb) (__memb).source.buf, (__memb).size

int main(int argc, const char **argv)
{
    if (argc <= 1 || strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "--help", 6) == 0) {
        showusage(stdout);
        exit(EXIT_SUCCESS);
    }

    if (strncmp(argv[1], "--version", 9) == 0) {
        printf("%s%s\n", VERSION, REVISION);
        exit(EXIT_SUCCESS);
    }

    args   args    = parseargs(argv);
    string cfgfile = tryfload(args.config);
    string csvfile = tryfload(args.files);
    FILE  *outfile = NULL;
    if (!args.dryrun) {
        outfile = fopen(args.outfile, "wb");
        if (!outfile) die("could not open output file “%s”!", args.outfile);
    }

    chdir(args.workdir);

    rompacker *packer = rompacker_new((unsigned int)args.verbose);
    dieiferr(cfgparse(cfgfile, cfgsections, packer), cfgresult);
    dieiferr(csvparse(csvfile, NULL, csv_addfile, packer), sheetsresult);

    if (rompacker_seal(packer) != 0) {
        int maxshift = packer->prom ? MAX_CAPSHIFT_PROM : MAX_CAPSHIFT_MROM;
        die("computed ROM size exceeds allowable maximum of 0x%08X!\n",
            TRY_CAPSHIFT_BASE << maxshift);
    }

    if (args.dryrun) {
        fdump("header.sbin", dumpargs(packer->header));
        fdump("banner.sbin", dumpargs(packer->banner));
        fdump("fntb.sbin", dumpargs(packer->fntb));
        fdump("fatb.sbin", dumpargs(packer->fatb));
    } else {
        enum dumperr err = rompacker_dump(packer, outfile);
        switch (err) {
        case E_dump_packing: die("packer was not correctly sealed!");
        case E_dump_ok:      break;
        }
    }

    rompacker_del(packer);
    free(cfgfile.s);
    free(csvfile.s);
    if (outfile) fclose(outfile);
    exit(EXIT_SUCCESS);
}

static args parseargs(const char **argv)
{
    args args    = { 0 };
    args.workdir = ".";
    args.outfile = "rom.nds";

    // clang-format off
    const clipopt options[] = {
        { .longopt = "directory", .shortopt = 'C', .hasarg = H_reqarg, .starget = &args.workdir },
        { .longopt = "output",    .shortopt = 'o', .hasarg = H_reqarg, .starget = &args.outfile },
        { .longopt = "dry-run",   .shortopt = 'd', .hasarg = H_noarg,  .ntarget = &args.dryrun  },
        { .longopt = "verbose",   .shortopt = 'v', .hasarg = H_noarg,  .ntarget = &args.verbose },
        { 0 },
    };

    const clippos positionals[] = {
        { .name = "config",  .target = &args.config },
        { .name = "filesys", .target = &args.files  },
        { 0 },
    };
    // clang-format on

    clip clip = clipinit(argv);
    if (cliparse(&clip, options, positionals, NULL)) dieusage("%s", clip.err);
    return args;
}

static void showusage(FILE *stream)
{
    fprintf(stream, "nitrorom - Produce a Nintendo DS ROM from sources\n");
    fprintf(stream, "\n");
    fprintf(stream, "Usage: nitrorom [OPTIONS] <CONFIG.INI> <FILESYS.CSV>\n");
    fprintf(stream, "\n");
    fprintf(stream, "For details on the precise format of CONFIG.INI and FILESYS.CSV, refer to\n");
    fprintf(stream, "this program's manual page.\n");
    fprintf(stream, "\n");
    fprintf(stream, "Program Information (must be specified first):\n");
    fprintf(stream, "  -h / --help            Display this help-text and exit.\n");
    fprintf(stream, "  --version              Display the program's version number and exit.\n");
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -C / --directory DIR   Change to directory DIR before loading any files.\n");
    fprintf(stream, "  -o / --output FILE     Write the output ROM to FILE. Default: “rom.nds”.\n");
    fprintf(stream, "  -d / --dry-run         Enable dry-run mode; do not create an output ROM\n");
    fprintf(stream, "                         and instead emit computed artifacts: the ROM's\n");
    fprintf(stream, "                         header, banner, and filesystem tables.\n");
    fprintf(stream, "  -v / --verbose         Enable verbose mode; emit additional program logs\n");
    fprintf(stream, "                         during execution to standard-error.\n");
}

static string tryfload(const char *filename)
{
    string fcont = fload(filename);
    if (fcont.len < 0) die("could not load input file “%s”: %s", filename, strerror(errno));
    return fcont;
}
