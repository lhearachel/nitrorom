#include <stdarg.h>
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
    u16 securecrc;
} Options;

// clang-format off
static const char *tagline = "nitrorom - Create Nintendo DS ROM images\n";
static const char *usagespec = ""
    "Usage: nitrorom [OPTION]... SPECFILE\n"
    "       nitrorom [-V / --version]\n"
    "       nitrorom [-h / --help]\n"
    "";
static const char *options = ""
    "Options:\n"
    "  -c / --secure-crc <crc>  Specify the secure area checksum to be stored in\n"
    "                           the output ROM's header. This option accepts input\n"
    "                           in decimal, hexadecimal (with leading “0x”), or\n"
    "                           octal (with leading “0”).\n"
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

static inline __attribute__((format(printf, 1, 2))) void errusage(const char *fmt, ...)
{
    va_list argv;
    va_start(argv, fmt);

    fputs("nitrorom: ", stderr);
    vfprintf(stderr, fmt, argv);
    fputc('\n', stderr);
    usage(stderr);

    va_end(argv);
    exit(EXIT_FAILURE);
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
        .securecrc = 0xFFFF,
    };

    while (argc > 1 && isopt(*argv)) {
        char *opt = *argv;
        argc--;
        argv++;

        if (matchopt(opt, "", "--dry-run")) {
            opts.dryrun = true;
        } else if (matchopt(opt, "-c", "--secure-crc")) {
            char *invalid = "";
            opts.securecrc = strtol(*argv, &invalid, 0);

            if (*invalid != '\0') {
                errusage("expected numeric argument for “%s”, but found “%s”\n", opt, invalid);
            }

            argc--;
            argv++;
        } else if (matchopt(opt, "-C", "--directory")) {
            opts.workdir = *argv;
            argc--;
            argv++;
        } else if (matchopt(opt, "-o", "--output")) {
            opts.outfile = *argv;
            argc--;
            argv++;
        } else {
            errusage("unrecognized option “%s”\n", opt);
        }
    }

    if (argc == 0) {
        errusage("missing required positional argument SPECFILE\n");
    }

    opts.specfile = *argv;
    return opts;
}

static void dryline(MemberOffsets offsets, u16 fileid, const char *source, const char *target)
{
    printf("%08X,%08X,%04X,\"%s\",\"%s\"\n", offsets.begin, offsets.end, fileid, source, target);
}

static void drydump(ROMSpec *spec, ROMLayout *layout, ROMOffsets *offsets)
{
    u32 fileid = 0;

    dryline((MemberOffsets){0, 0x4000}, 0xFFFF, spec->properties.header_fpath, "*HEADER*");

    dryline(offsets->arm9, 0xFFFF, spec->arm9.code_binary_fpath, "*ARM9*");
    if (layout->arm9_defs.num_overlays > 0) {
        dryline(offsets->ovy9, 0xFFFF, spec->arm9.overlay_table_fpath, "*OVY9*");
        for (u32 i = 0; i < layout->arm9_defs.num_overlays; i++, fileid++) {
            char ovyid[16];
            sprintf(ovyid, "*OVY9_%04X*", i);
            dryline(offsets->filesys[fileid], fileid, layout->arm9_defs.overlay_filenames[i], "*");
        }
    }

    dryline(offsets->arm7, 0xFFFF, spec->arm7.code_binary_fpath, "*ARM7*");
    if (layout->arm7_defs.num_overlays > 0) {
        dryline(offsets->ovy7, 0xFFFF, spec->arm7.overlay_table_fpath, "*OVY7*");
        for (u32 i = 0; i < layout->arm7_defs.num_overlays; i++, fileid++) {
            char ovyid[16];
            sprintf(ovyid, "*OVY7_%04X*", i);
            dryline(offsets->filesys[fileid], fileid, layout->arm7_defs.overlay_filenames[i], "*");
        }
    }

    dryline(offsets->banner, 0xFFFF, spec->properties.banner_fpath, "*BANNER*");
    dryline(offsets->fntb, 0xFFFF, "*FILENAMES*", "*FNTB*");
    dryline(offsets->fatb, 0xFFFF, "*FILEALLOC*", "*FATB*");
    for (u32 i = 0; i < spec->len_files; i++, fileid++) {
        dryline(offsets->filesys[fileid], spec->files[i].filesys_id, spec->files[i].source_path, spec->files[i].target_path);
    }
}

int main(int argc, char **argv)
{
    Options opts = parseopts(argc, argv);
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

    char cwd[256];
    getcwd(cwd, 256);
    chdir(opts.workdir);
    LayoutResult laidout = compute_rom_layout(parsed.spec);
    ROM rom = makerom(parsed.spec, laidout.layout, opts.securecrc, opts.dryrun);

    if (opts.dryrun) {
        drydump(parsed.spec, laidout.layout, rom.offsets);
        free(rom.offsets);
    } else {
        chdir(cwd);
        FILE *rom_f = fopen(opts.outfile, "wb");
        fwrite(rom.buffer.data, 1, rom.buffer.size, rom_f);
        fclose(rom_f);
        free(rom.buffer.data);
    }

    dspec(parsed.spec);
    dlayout(laidout.layout);
}
