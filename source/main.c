// SPDX-License-Identifier: MIT

/*
 * nitrorom-pack - Produce a Nintendo DS ROM from sources
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clip.h"
#include "config.h"
#include "packer.h"
#include "sheets.h"
#include "strings.h"

#define die(__msg, ...)                      \
    {                                        \
        fputs("nitrorom: ", stderr);         \
        fprintf(stderr, __msg, __VA_ARGS__); \
        fputc('\n', stderr);                 \
        exit(EXIT_FAILURE);                  \
    }

#define dieusage(__msg, ...)                 \
    {                                        \
        fputs("nitrorom: ", stderr);         \
        fprintf(stderr, __msg, __VA_ARGS__); \
        fputs("\n\n", stderr);               \
        showusage(stderr);                   \
        exit(EXIT_FAILURE);                  \
    }

#define dieiferr(__cond, __resT, __msg)             \
    {                                               \
        __resT __res = __cond;                      \
        if (__res.code != 0) die(__msg, __res.msg); \
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
    { .section = string("cpu.arm9"), .handler = cfg_arm9   },
    { .section = string("cpu.arm7"), .handler = cfg_arm7   },
    { .section = stringZ,            .handler = NULL       },
};
// clang-format on

static void   showusage(FILE *stream);
static args   parseargs(const char **argv);
static string fload(const char *filename);

int main(int argc, const char **argv)
{
    if (argc <= 1 || strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "--help", 6) == 0) {
        showusage(stdout);
        exit(EXIT_SUCCESS);
    }

    args args = parseargs(argv);

#ifndef NDEBUG
    printf("config:  %s\n", args.config);
    printf("files:   %s\n", args.files);
    printf("workdir: %s\n", args.workdir);
    printf("outfile: %s\n", args.outfile);
    printf("dryrun?  %s\n", args.dryrun ? "yes" : "no");
    printf("verbose? %s\n", args.verbose ? "yes" : "no");
#endif // NDEBUG

    string cfgfile = fload(args.config);
    string csvfile = fload(args.files);

    rompacker *packer = rompacker_new((unsigned int)args.verbose);

    dieiferr(cfgparse(cfgfile, cfgsections, packer), cfgresult, "config error - %s");
    dieiferr(csvparse(csvfile, NULL, csv_addfile, packer), sheetsresult, "sheets error - %s");

    rompacker_seal(packer);
    rompacker_dump(packer, NULL); // TODO:
    rompacker_del(packer);
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
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -C / --directory DIR   Change to directory DIR before loading any files.\n");
    fprintf(stream, "  -o / --output FILE     Write the output ROM to FILE. Default: “rom.nds”.\n");
    fprintf(stream, "  -d / --dry-run         Enable dry-run mode; do not create an output ROM\n");
    fprintf(stream, "                         and instead emit computed artifacts: the ROM's\n");
    fprintf(stream, "                         header, banner, and filesystem tables.\n");
    fprintf(stream, "  -v / --verbose         Enable verbose mode; emit additional program logs\n");
    fprintf(stream, "                         during execution to standard-error.\n");
}

static string fload(const char *filename)
{
    FILE *infp = fopen(filename, "rb");
    if (!infp) die("could not open input file “%s”: %s", filename, strerror(errno));

    fseek(infp, 0, SEEK_END);
    long fsize = ftell(infp);
    fseek(infp, 0, SEEK_SET);

    if (fsize < 0) {
        fclose(infp);
        die("could not get size of input file “%s”: %s", filename, strerror(errno));
    }

    string fcont = string(calloc(fsize, 1), fsize);
    fread(fcont.s, 1, fsize, infp);
    fclose(infp);
    return fcont;
}
