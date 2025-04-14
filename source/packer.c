// SPDX-License-Identifier: MIT

#include "packer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "fileio.h"
#include "litend.h"
#include "sheets.h"
#include "strings.h"
#include "vector.h"

#define OFS_HEADER_ARM9_ROMOFFSET   0x020
#define OFS_HEADER_ARM7_ROMOFFSET   0x030
#define OFS_HEADER_FNTB_ROMOFFSET   0x040
#define OFS_HEADER_FNTB_BSIZE       0x044
#define OFS_HEADER_FATB_ROMOFFSET   0x048
#define OFS_HEADER_FATB_BSIZE       0x04C
#define OFS_HEADER_OVT9_ROMOFFSET   0x050
#define OFS_HEADER_OVT9_BSIZE       0x054
#define OFS_HEADER_OVT7_ROMOFFSET   0x058
#define OFS_HEADER_OVT7_BSIZE       0x05C
#define OFS_HEADER_BANNER_ROMOFFSET 0x068

rompacker *rompacker_new(unsigned int verbose)
{
    rompacker *packer = calloc(1, sizeof(*packer));
    if (!packer) return 0;

    packer->packing = 1;
    packer->verbose = verbose;

    // The header is the only constant-size element in the entire ROM, so we can pre-allocate it.
    packer->banner.source.filename = string("%HEADER%");
    packer->header.source.size     = HEADER_BSIZE;
    packer->header.source.buf      = calloc(HEADER_BSIZE, 1);

    packer->ovy9    = newvec(rommember, 128);
    packer->ovy7    = newvec(rommember, 128);
    packer->filesys = newvec(romfile, 512);

    return packer;
}

static inline void safeclose(FILE *f)
{
    if (f) fclose(f);
}

void rompacker_del(rompacker *packer)
{
    // These are allocated by `cfg_overlays` in `cfg_arm.c`.
    if (packer->ovy9.len > 0) free(get(&packer->ovy9, rommember, 0)->source.filename.s);
    if (packer->ovy7.len > 0) free(get(&packer->ovy7, rommember, 0)->source.filename.s);

    free(packer->header.source.buf);
    free(packer->banner.source.buf);
    free(packer->fntb.source.buf);
    free(packer->fatb.source.buf);
    free(packer->ovy9.data);
    free(packer->ovy7.data);
    free(packer->filesys.data);

    safeclose(packer->arm9.source.hdl);
    safeclose(packer->ovt9.source.hdl);
    safeclose(packer->arm7.source.hdl);
    safeclose(packer->ovt7.source.hdl);

    free(packer);
}

static int comparefnames(const void *a, const void *b) // NOLINT
{
    const romfile *file1 = a;
    const romfile *file2 = b;

    string tpath1 = file1->target;
    string tpath2 = file2->target;
    int    result = 0;

    do {
        strpair tcomp1 = strcut(tpath1, '/');
        strpair tcomp2 = strcut(tpath2, '/');

        // Subdirectories are always sorted after files at the same depth.
        // e.g., `/data/sound/<file>` is always sorted after `/data/<file>`
        unsigned char tterm1 = tcomp1.tail.len > 0 ? tcomp1.head.s[tcomp1.head.len] : '\0';
        unsigned char tterm2 = tcomp2.tail.len > 0 ? tcomp2.head.s[tcomp2.head.len] : '\0';

        result = tterm1 == tterm2 ? stricmp(tcomp1.head, tcomp2.head) : tterm1 == '\0' ? -1 : 1;
        tpath1 = tcomp1.tail;
        tpath2 = tcomp2.tail;
    } while (tpath1.len > 0 && tpath2.len > 0 && result == 0);

    return result;
}

#define fatb_begin(__fatb, __i) ((__fatb) + ((ptrdiff_t)((__i) * 8)))
#define fatb_end(__fatb, __i)   ((__fatb) + ((ptrdiff_t)((__i) * 8) + 4))

#define membsize(__memb) ((__memb)->source.size + (__memb)->pad)
#define filesize(__file) ((__file)->size + (__file)->pad)

#define printmemb(__curs, __memb)                              \
    {                                                          \
        fprintf(                                               \
            stderr,                                            \
            "rompacker:member: 0x%08X,0x%08lX,0x%08lX,%.*s\n", \
            __curs,                                            \
            (__memb)->source.size,                             \
            membsize(__memb),                                  \
            fmtstring((__memb)->source.filename)               \
        );                                                     \
    }

#define printfile(__curs, __file)                              \
    {                                                          \
        fprintf(                                               \
            stderr,                                            \
            "rompacker:member: 0x%08X,0x%08lX,0x%08lX,%.*s\n", \
            __curs,                                            \
            (__file)->size,                                    \
            filesize(__file),                                  \
            fmtstring((__file)->target)                        \
        );                                                     \
    }

#define sealarmparams(__packer, __n)                                         \
    &(__packer)->arm##__n, &(__packer)->ovt##__n, &(__packer)->ovy##__n, __n

static void sealarm(
    rommember     *arm,
    rommember     *ovt,
    vector        *ovyvec,
    int            which,
    unsigned char *header, // NOLINT
    unsigned char *fatb,   // NOLINT
    uint32_t      *romcursor,
    int            ovyofs, // NOLINT
    int            verbose // NOLINT
)
{
    uint32_t ofsarmrom  = which == 9 ? OFS_HEADER_ARM9_ROMOFFSET : OFS_HEADER_ARM7_ROMOFFSET;
    uint32_t ofsovtrom  = which == 9 ? OFS_HEADER_OVT9_ROMOFFSET : OFS_HEADER_OVT7_ROMOFFSET;
    uint32_t ofsovtsize = which == 9 ? OFS_HEADER_OVT9_BSIZE : OFS_HEADER_OVT7_BSIZE;

    if (verbose) printmemb(*romcursor, arm);
    putleword(header + ofsarmrom, *romcursor);
    *romcursor = *romcursor + membsize(arm);

    if (verbose) printmemb(*romcursor, ovt);
    putleword(header + ofsovtrom, *romcursor);
    putleword(header + ofsovtsize, *romcursor);
    *romcursor = *romcursor + membsize(ovt);

    for (int i = 0, j = ovyofs; i < ovyvec->len; i++, j++) {
        rommember *ovy = get(ovyvec, rommember, i);
        putleword(fatb_begin(fatb, j), *romcursor);
        putleword(fatb_end(fatb, j), *romcursor + ovy->source.size);

        if (verbose) printmemb(*romcursor, ovy);
        *romcursor = *romcursor + membsize(ovy);
    }
}

static void sealfntb(rompacker *packer, romfile *sorted, uint32_t fileid)
{
    (void)sorted;
    (void)fileid;

    // TODO: Hacking the FNTB for now
    packer->fntb.source.filename = string("%FILENAMES%");
    packer->fntb.source.size     = 0x1BB4;
    packer->fntb.source.buf      = calloc(packer->fntb.source.size, 1);
    packer->fntb.pad             = 0x4C;
}

void rompacker_seal(rompacker *packer)
{
    if (packer->verbose) fprintf(stderr, "rompacker: sealing the packer...\n");

    packer->packing = 0;

    int numovys  = packer->ovy9.len + packer->ovy7.len;
    int numfiles = numovys + packer->filesys.len;
    if (numfiles > 0) {
        packer->fatb.source.filename = string("%FILEALLOCS%");
        packer->fatb.source.size     = (long)numfiles * 8;
        packer->fatb.source.buf      = calloc(packer->fatb.source.size, 1);
        packer->fatb.pad             = -(packer->fatb.source.size) & (ROM_ALIGN - 1);
    }

    uint32_t       romcursor = HEADER_BSIZE;
    unsigned char *fatb      = packer->fatb.source.buf;
    unsigned char *header    = packer->header.source.buf;
    sealarm(sealarmparams(packer, 9), header, fatb, &romcursor, 0, packer->verbose);
    sealarm(sealarmparams(packer, 7), header, fatb, &romcursor, packer->ovy9.len, packer->verbose);

    if (packer->filesys.len > 0) {
        romfile *sorted = malloc(sizeof(romfile) * packer->filesys.len);
        memcpy(sorted, packer->filesys.data, sizeof(romfile) * packer->filesys.len);
        qsort(sorted, packer->filesys.len, sizeof(romfile), comparefnames);
        sealfntb(packer, sorted, numovys);
        free(sorted);
    }

    if (packer->verbose) printmemb(romcursor, &packer->fntb);
    putleword(header + OFS_HEADER_FNTB_ROMOFFSET, romcursor);
    putleword(header + OFS_HEADER_FNTB_BSIZE, packer->fntb.source.size);
    romcursor += membsize(&packer->fntb);

    if (packer->verbose) printmemb(romcursor, &packer->fatb);
    putleword(header + OFS_HEADER_FATB_ROMOFFSET, romcursor);
    putleword(header + OFS_HEADER_FATB_BSIZE, packer->fatb.source.size);
    romcursor += membsize(&packer->fatb);

    if (packer->verbose) printmemb(romcursor, &packer->banner);
    putleword(header + OFS_HEADER_BANNER_ROMOFFSET, romcursor);
    romcursor += membsize(&packer->banner);

    for (int i = 0, j = numovys; i < packer->filesys.len; i++, j++) {
        romfile *topack = get(&packer->filesys, romfile, i);
        if (packer->verbose) printfile(romcursor, topack);
        putleword(fatb_begin(fatb, j), romcursor);
        putleword(fatb_end(fatb, j), romcursor + topack->size);
        romcursor += filesize(topack);
    }

    // TODO: Compute CRCs for header and banner

    if (packer->verbose) fprintf(stderr, "rompacker: packer is sealed, okay to dump!\n");
}

enum dumperr rompacker_dump(rompacker *packer, FILE *stream)
{
    if (packer->verbose) fprintf(stderr, "rompacker: dumping contents to disk!\n");
    if (packer->packing) return E_dump_packing;
    if (!stream) return E_dump_nullfile;

    // TODO: Write members to stream
    return E_dump_ok;
}

#define sheetserr(__msg, ...)                                           \
    {                                                                   \
        sheetsresult __res = { .code = E_sheets_user, .pos = stringZ }; \
        snprintf(                                                       \
            (__res).msg,                                                \
            sizeof(__res).msg,                                          \
            "rompacker:filesystem:%d: " __msg,                          \
            line,                                                       \
            __VA_ARGS__                                                 \
        );                                                              \
        return __res;                                                   \
    }

#define SOURCE 0
#define TARGET 1

sheetsresult csv_addfile(sheetsrecord *record, void *user, int line)
{
    if (record->nfields != 2) {
        sheetserr("expected 2 fields for record, but found %lu", record->nfields);
    }

    rompacker *packer = user;
    romfile   *file   = push(&packer->filesys, romfile);
    file->source      = record->fields[SOURCE];
    file->target      = record->fields[TARGET];
    file->size        = fsizes(file->source);
    file->pad         = (int)(-(file->size) & (ROM_ALIGN - 1));

    if (file->size < 0) sheetserr("could not open source file “%.*s”", fmtstring(file->source));

    if (packer->verbose) {
        fprintf(
            stderr,
            "rompacker:filesystem: 0x%08lX,0x%08X,%.*s,%.*s\n",
            file->size,
            file->pad,
            fmtstring(file->source),
            fmtstring(file->target)
        );
    }

    return (sheetsresult){ .code = E_sheets_none };
}
