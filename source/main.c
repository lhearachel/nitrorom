// SPDX-License-Identifier: MIT

/*
 * nitrorom-pack - Produce a Nintendo DS ROM from sources
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clip.h"

#define die(__msg, ...)                      \
    {                                        \
        fputs("nitropack: ", stderr);        \
        fprintf(stderr, __msg, __VA_ARGS__); \
        fputc('\n', stderr);                 \
        exit(EXIT_FAILURE);                  \
    }

typedef struct args {
    const char *config;
    const char *files;
    const char *workdir;
    const char *outfile;

    long dryrun;
    long verbose;
} args;

static const char *tagline  = "nitrorom - Produce a Nintendo DS ROM from sources\n";
static const char *synopsis = "Usage: nitrorom [OPTIONS] <CONFIG.INI> <FILESYS.CSV>\n";
static const char *descript
    = "For details on the precise format of CONFIG.INI and FILESYS.CSV, refer to\n"
      "this program's manual page.\n";
static const char *options
    = "Options:\n"
      "  -C / --directory DIR   Change to directory DIR before loading any files.\n"
      "  -o / --output FILE     Write the output ROM to FILE. Default: “rom.nds”.\n"
      "  -d / --dry-run         Enable dry-run mode; do not create an output ROM\n"
      "                         and instead emit computed artifacts: the ROM's\n"
      "                         header, banner, and filesystem tables.\n"
      "  -v / --verbose         Enable verbose mode; emit additional program logs\n"
      "                         during execution to standard-error.\n";

static args parseargs(const char **argv);

int main(int argc, const char **argv)
{
    if (argc <= 1 || strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "--help", 6) == 0) {
        printf("%s\n%s\n%s\n%s\n", tagline, synopsis, descript, options);
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
    if (cliparse(&clip, options, positionals, NULL)) die("%s", clip.err);
    return args;
}
