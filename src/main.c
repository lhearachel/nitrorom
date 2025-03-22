#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "errout.h"
#include "io.h"
#include "layout.h"
#include "lexer.h"
#include "makerom.h"
#include "parser.h"

typedef struct Options {
    char *workdir;
    char *outfile;
    char *specfile;
    bool dryrun;
} Options;

// clang-format off
static const char *tagline = "ndsmake - Create Nintendo DS-compatible ROM files\n";
static const char *usagespec = ""
    "Usage: ndsmake [OPTION]... SPECFILE\n"
    "       ndsmake [-V / --version]\n"
    "       ndsmake [-h / --help]\n"
    "";
static const char *options = ""
    "Options:\n"
    "  -C / --directory <dir>   Change the working directory before reading input\n"
    "                           files for insertion into the output ROM. This will\n"
    "                           NOT affect loading the spec-file input.\n"
    "  -o / --output <file>     Write the output ROM to a specific file. If not\n"
    "                           specified, then the output ROM will be written to\n"
    "                           “rom.nds” in the working directory."
    "";
// clang-format on

static inline bool matchopt(char *opt, char *s, char *l)
{
    return strcmp(opt, s) == 0 || strcmp(opt, l) == 0;
}

static inline bool isopt(char *s)
{
    return s[0] == '-' && (s[1] != '-' || s[2] != '\0');
}

static inline void usage(FILE *stream)
{
    fprintf(stream, "%s\n%s\n%s", tagline, usagespec, options);
}

static Options parseopts(int argc, char **argv)
{
    argc--;
    argv++;

    if (argc == 0 || matchopt(*argv, "-h", "--help")) {
        usage(stdout);
        exit(EXIT_SUCCESS);
    }

    if (matchopt(*argv, "-V", "--version")) {
        printf("0.1.0\n");
        exit(EXIT_SUCCESS);
    }

    Options opts = {
        .workdir = ".",
        .outfile = "rom.nds",
        .specfile = null,
        .dryrun = false,
    };

    while (argc > 1 && isopt(*argv)) {
        char *opt = *argv;
        argc--;
        argv++;

        if (matchopt(opt, "", "--dry-run")) {
            opts.dryrun = true;
        } else if (matchopt(opt, "-C", "--directory")) {
            opts.workdir = *argv;
            argc--;
            argv++;
        } else if (matchopt(opt, "-o", "--output")) {
            opts.outfile = *argv;
            argc--;
            argv++;
        } else {
            fprintf(stderr, "ndsmake: unrecognized option “%s”\n\n", opt);
            usage(stderr);
            exit(EXIT_FAILURE);
        }
    }

    if (argc == 0) {
        fprintf(stderr, "ndsmake: missing required positional argument SPECFILE\n\n");
        usage(stderr);
        exit(EXIT_FAILURE);
    }

    opts.specfile = *argv;
    return opts;
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        fprintf(stderr, "Usage: ndsmake SPECFILE\n");
    }

    Options opts = parseopts(argc, argv);

#ifndef NDEBUG
    printf("Work directory: “%s”\n", opts.workdir);
    printf("Output file:    “%s”\n", opts.outfile);
    printf("Spec file:      “%s”\n", opts.specfile);
#endif

    String source = fload(opts.specfile);
    LexResult lexed = lex(source.p, source.len);
    if (!lexed.ok) {
        report_lex_err(opts.specfile, source.p, &lexed);
        exit(EXIT_FAILURE);
    }

    ParseResult parsed = parse(lexed.tokens, lexed.len, source.p);
    if (!parsed.ok) {
        report_parse_err(opts.specfile, source.p, &parsed, &lexed);
        free(lexed.tokens);
        free(source.p);
        exit(EXIT_FAILURE);
    }
    free(lexed.tokens);
    free(source.p);

    chdir(opts.workdir);
    LayoutResult laidout = compute_rom_layout(parsed.spec);
    byte *fnt = makefnt(laidout.layout->filesystem, laidout.layout->fnt_size);
    makerom(parsed.spec, laidout.layout, fnt, opts.dryrun);

    if (opts.dryrun) {
        FILE *fnt_f = fopen("fnt.sbin", "wb");
        fwrite(fnt, 1, laidout.layout->fnt_size, fnt_f);
        fclose(fnt_f);
    }

    dspec(parsed.spec);
    dlayout(laidout.layout);
    free(fnt);
}
