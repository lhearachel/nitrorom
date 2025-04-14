// SPDX-License-Identifier: MIT

#include "packer.h"

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "constants.h"
#include "fileio.h"
#include "sheets.h"
#include "strings.h"
#include "vector.h"

rompacker *rompacker_new(unsigned int verbose)
{
    rompacker *packer = calloc(1, sizeof(*packer));
    if (!packer) return 0;

    packer->packing = 1;
    packer->verbose = verbose;

    // The header is the only constant-size element in the entire ROM, so we
    // can pre-allocate it.
    packer->header.source.size = HEADER_BSIZE;
    packer->header.source.buf  = calloc(HEADER_BSIZE, 1);

    // Technically, DSi-mode banners support an alternate size. That is a problem for the future.
    packer->banner.source.size = BANNER_BSIZE;
    packer->banner.source.buf  = calloc(BANNER_BSIZE, 1);
    packer->banner.pad         = -BANNER_BSIZE & (ROM_ALIGN - 1);

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

void rompacker_seal(rompacker *packer)
{
    if (packer->verbose) fprintf(stderr, "rompacker: sealing the packer...\n");

    packer->packing = 0;
    // TODO: Compute FNTB
    // TODO: Compute FATB
    // TODO: Update offsets in packer header
    // TODO: Compute CRCs in header and banner

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
    file->pad         = -(file->size) & (ROM_ALIGN - 1);

    if (file->size < 0) sheetserr("could not open source file “%.*s”", fmtstring(file->source));

    if (packer->verbose) {
        fprintf(
            stderr,
            "rompacker:filesystem: 0x%08lX,0x%08lX,%.*s,%.*s\n",
            file->size,
            file->pad,
            fmtstring(file->source),
            fmtstring(file->target)
        );
    }

    return (sheetsresult){ .code = E_sheets_none };
}

cfgresult cfg_arm9(string sec, string key, string val, void *user, long line) // NOLINT
{
    (void)sec;
    (void)line;

    rompacker *packer = user;
    if (packer->verbose) {
        fprintf(
            stderr,
            "rompacker:configuration:arm9 “%.*s” -> “%.*s”\n",
            fmtstring(key),
            fmtstring(val)
        );
    }

    return (cfgresult){ .code = E_config_none };
}

cfgresult cfg_arm7(string sec, string key, string val, void *user, long line) // NOLINT
{
    (void)sec;
    (void)line;

    rompacker *packer = user;
    if (packer->verbose) {
        fprintf(
            stderr,
            "rompacker:configuration:arm7 “%.*s” -> “%.*s”\n",
            fmtstring(key),
            fmtstring(val)
        );
    }

    return (cfgresult){ .code = E_config_none };
}
