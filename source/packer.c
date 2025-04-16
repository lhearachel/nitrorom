// SPDX-License-Identifier: MIT

#include "packer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "litend.h"
#include "strings.h"
#include "vector.h"

rompacker *rompacker_new(unsigned int verbose)
{
    rompacker *packer = calloc(1, sizeof(*packer));
    if (!packer) return 0;

    packer->packing = 1;
    packer->verbose = verbose;

    // The header is the only constant-size element in the entire ROM, so we can pre-allocate it.
    packer->banner.source.filename = string("%HEADER%");
    packer->header.source.buf      = calloc(HEADER_BSIZE, 1);
    packer->header.size            = HEADER_BSIZE;

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

static uint16_t crctable[16] = {
    0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
    0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400,
};

static uint16_t crc16(string data, uint16_t crc)
{
    const unsigned char *end = data.s + data.len;

    uint16_t x = 0;
    uint16_t y;
    uint16_t bit = 0;
    while (data.s < end) {
        if (bit == 0) x = lehalf(data.s);

        y     = crctable[crc & 15];
        crc >>= 4;
        crc  ^= y;
        crc  ^= crctable[(x >> bit) & 15];
        bit  += 4;

        if (bit == 16) {
            data.s += 2;
            bit     = 0;
        }
    }

    return crc;
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

#define membsize(__memb) ((__memb)->size + (__memb)->pad)

#define printmemb(__curs, __memb)                            \
    {                                                        \
        fprintf(                                             \
            stderr,                                          \
            "rompacker:member: 0x%08X,0x%08X,0x%08X,%.*s\n", \
            __curs,                                          \
            (__memb)->size,                                  \
            membsize(__memb),                                \
            fmtstring((__memb)->source.filename)             \
        );                                                   \
    }

#define printfile(__curs, __file)                            \
    {                                                        \
        fprintf(                                             \
            stderr,                                          \
            "rompacker:member: 0x%08X,0x%08X,0x%08X,%.*s\n", \
            __curs,                                          \
            (__file)->size,                                  \
            membsize(__file),                                \
            fmtstring((__file)->target)                      \
        );                                                   \
    }

#define sealarmparams(__packer, __n)                                         \
    &(__packer)->arm##__n, &(__packer)->ovt##__n, &(__packer)->ovy##__n, __n

#define sealmemb(__memb, __cursor, __verbose)             \
    {                                                     \
        if (__verbose) printmemb(__cursor, __memb);       \
        (__memb)->offset = (__cursor);                    \
        (__cursor)       = (__cursor) + membsize(__memb); \
    }

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

    putleword(header + ofsarmrom, *romcursor);
    sealmemb(arm, *romcursor, verbose);

    putleword(header + ofsovtrom, ovt->size > 0 ? *romcursor : 0);
    putleword(header + ofsovtsize, ovt->size);
    sealmemb(ovt, *romcursor, verbose);

    for (int i = 0, j = ovyofs; i < ovyvec->len; i++, j++) {
        rommember *ovy = get(ovyvec, rommember, i);
        putleword(fatb_begin(fatb, j), *romcursor);
        putleword(fatb_end(fatb, j), *romcursor + ovy->size);
        sealmemb(ovy, *romcursor, verbose);
    }
}

static void sealfntb(rompacker *packer, romfile *sorted, uint32_t fileid)
{
    (void)sorted;
    (void)fileid;

    // TODO: Hacking the FNTB for now
    packer->fntb.source.filename = string("%FILENAMES%");
    packer->fntb.size            = 0x1BB4;
    packer->fntb.source.buf      = calloc(packer->fntb.size, 1);
    packer->fntb.pad             = 0x4C;
}

static int sealheader(rompacker *packer, uint32_t romsize)
{
    unsigned char *header = packer->header.source.buf;

    uint32_t trycap   = TRY_CAPSHIFT_BASE;
    int      maxshift = packer->prom ? MAX_CAPSHIFT_PROM : MAX_CAPSHIFT_MROM;
    int      shift    = 0;
    for (; shift < maxshift; shift++) {
        if (romsize < (trycap << shift)) {
            header[OFS_HEADER_CHIPCAPACITY] = shift;
            break;
        }
    }

    // ROM size exceeds the maximum; main thread should die from here
    if (shift == maxshift) return 1;

    putleword(header + OFS_HEADER_ROMSIZE, romsize);
    putleword(header + OFS_HEADER_HEADERSIZE, HEADER_BSIZE);
    putleword(header + OFS_HEADER_STATICFOOTER, 0x00004BA0); // static NitroSDK footer

    uint16_t crc = crc16(string(header, OFS_HEADER_HEADERCRC), 0xFFFF);
    if (packer->verbose) fprintf(stderr, "rompacker: header CRC: 0x%04X\n", crc);
    putlehalf(header + OFS_HEADER_HEADERCRC, crc);

    if (packer->verbose) {
        fprintf(
            stderr,
            "rompacker: storage: 0x%08X used / 0x%08X avail (%f%%)\n",
            romsize,
            (trycap << shift),
            romsize / (double)(trycap << shift)
        );
    }

    return 0;
}

static void sealbanner(rompacker *packer)
{
    unsigned char *banner = packer->banner.source.buf;

    uint16_t crc = crc16(string(banner + 0x20, BANNER_BSIZE_V1 - 0x20), 0xFFFF);
    putleword(banner + OFS_BANNER_CRC_V1OFFSET, crc);
    if (packer->verbose) fprintf(stderr, "rompacker: banner v1 CRC: 0x%04X\n", crc);

    if (packer->bannerver > 1) {
        crc = crc16(string(banner + 0x20, BANNER_BSIZE_V2 - 0x20), 0xFFFF);
        putleword(banner + OFS_BANNER_CRC_V2OFFSET, crc);
        if (packer->verbose) fprintf(stderr, "rompacker: banner v2 CRC: 0x%04X\n", crc);
    }

    if (packer->bannerver > 2) {
        crc = crc16(string(banner + 0x20, BANNER_BSIZE_V3 - 0x20), 0xFFFF);
        putleword(banner + OFS_BANNER_CRC_V3OFFSET, crc);
        if (packer->verbose) fprintf(stderr, "rompacker: banner v3 CRC: 0x%04X\n", crc);
    }
}

static inline void sealfileids(vector *unsorted, romfile *sorted, int firstfileid) // NOLINT
{
    for (int i = 0; i < unsorted->len; i++) {
        romfile *sfile   = sorted + i;
        romfile *ufile   = get(unsorted, romfile, sfile->packingid);
        ufile->filesysid = i + firstfileid;
    }
}

int rompacker_seal(rompacker *packer)
{
    if (packer->verbose) fprintf(stderr, "rompacker: sealing the packer...\n");

    packer->packing = 0;

    int numovys  = packer->ovy9.len + packer->ovy7.len;
    int numfiles = numovys + packer->filesys.len;
    if (numfiles > 0) {
        packer->fatb.source.filename = string("%FILEALLOCS%");
        packer->fatb.size            = numfiles * 8;
        packer->fatb.source.buf      = calloc(packer->fatb.size, 1);
        packer->fatb.pad             = -(packer->fatb.size) & (ROM_ALIGN - 1);
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
        sealfileids(&packer->filesys, sorted, numovys);
        free(sorted);
    }

    putleword(header + OFS_HEADER_FNTB_ROMOFFSET, romcursor);
    putleword(header + OFS_HEADER_FNTB_BSIZE, packer->fntb.size);
    sealmemb(&packer->fntb, romcursor, packer->verbose);

    putleword(header + OFS_HEADER_FATB_ROMOFFSET, romcursor);
    putleword(header + OFS_HEADER_FATB_BSIZE, packer->fatb.size);
    sealmemb(&packer->fatb, romcursor, packer->verbose);

    putleword(header + OFS_HEADER_BANNER_ROMOFFSET, romcursor);
    sealmemb(&packer->banner, romcursor, packer->verbose);

    for (int i = 0; i < packer->filesys.len; i++) {
        romfile *topack = get(&packer->filesys, romfile, i);
        putleword(fatb_begin(fatb, topack->filesysid), romcursor);
        putleword(fatb_end(fatb, topack->filesysid), romcursor + topack->size);

        // No need to write a macro for one instance.
        if (packer->verbose) printfile(romcursor, topack);
        topack->offset  = romcursor;
        romcursor      += membsize(topack);
    }

    // Final ROM size must ignore the padding of the last-added member (either the banner or the
    // last filesystem entry).
    uint32_t romsize = romcursor;
    if (packer->filesys.len > 0) {
        romsize -= get(&packer->filesys, romfile, packer->filesys.len - 1)->pad;
    } else {
        romsize -= packer->banner.pad;
    }

    sealbanner(packer);
    int result = sealheader(packer, romsize);
    if (packer->verbose) fprintf(stderr, "rompacker: packer is sealed, okay to dump!\n");
    return result;
}

enum dumperr rompacker_dump(rompacker *packer, FILE *stream)
{
    if (packer->verbose) fprintf(stderr, "rompacker: dumping contents to disk!\n");
    if (packer->packing) return E_dump_packing;
    if (!stream) return E_dump_nullfile;

    // TODO: Write members to stream
    return E_dump_ok;
}
