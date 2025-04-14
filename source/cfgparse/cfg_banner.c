// SPDX-License-Identifier: MIT

#include "packer.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfgparse.h"
#include "config.h"
#include "constants.h"
#include "fileio.h"
#include "litend.h"
#include "strings.h"

#define ICON_BITMAP_DIMEN  32
#define ICON_BITMAP_BSIZE  ((ICON_BITMAP_DIMEN * ICON_BITMAP_DIMEN) / 2)
#define ICON_COLOR_BSIZE   2
#define ICON_COLOR_DEPTH   16
#define ICON_PALETTE_BSIZE (ICON_COLOR_DEPTH * ICON_COLOR_BSIZE)

#define UNICODE_BMP_BSIZE  2
#define BANNER_TITLE_LEN   128
#define BANNER_TITLE_BSIZE (UNICODE_BMP_BSIZE * BANNER_TITLE_LEN)

#define OFS_BANNER_ICON_BITMAP  0x020
#define OFS_BANNER_ICON_PALETTE 0x220
#define OFS_BANNER_TITLE_JP     0x240
#define OFS_BANNER_TITLE_EN     0x340
#define OFS_BANNER_TITLE_FR     0x440
#define OFS_BANNER_TITLE_DE     0x540
#define OFS_BANNER_TITLE_IT     0x640
#define OFS_BANNER_TITLE_ES     0x740
#define OFS_BANNER_TITLE_CN     0x840
#define OFS_BANNER_TITLE_KR     0x940

static cfgresult cfg_banner_version(rompacker *packer, string val, long line)
{
    unsigned int result = 0;
    for (long i = 0; i < val.len; i++) {
        int digit = val.s[i] - '0';
        if (digit < 0 || digit > 10) {
            configerr(
                "expected unsigned base-10 numeric-literal, but found “%.*s”",
                fmtstring(val)
            );
        }

        result *= 10;
        result += digit;
    }

    long bannersize;
    switch (result) {
    case 1:  bannersize = BANNER_BSIZE_V1; break;
    case 2:  bannersize = BANNER_BSIZE_V2; break;
    case 3:  bannersize = BANNER_BSIZE_V3; break;
    default: configerr("expected banner version to be 1, 2, or 3, but found %d", result);
    }

    packer->bannerver              = result;
    packer->banner.source.filename = string("%BANNER%");
    packer->banner.source.size     = bannersize;
    packer->banner.pad             = -bannersize & (ROM_ALIGN - 1);
    packer->banner.source.buf      = calloc(bannersize, 1);

    unsigned char *banner = packer->banner.source.buf;
    banner[0]             = result;
    if (packer->verbose) {
        fprintf(stderr, "rompacker:configuration:banner: set version to %d\n", result);
    }

    return configok;
}

static cfgresult cfg_banner_icon4bpp(rompacker *packer, string val, long line)
{
    string ficon4bpp = floads(val);
    if (ficon4bpp.len < 0) configerr("could not open icon bitmap file “%.*s”", fmtstring(val));
    if (ficon4bpp.len > (long)ICON_BITMAP_BSIZE) {
        configerr(
            "icon bitmap file “%.*s” size 0x%08lX exceeds maximum size 0x%04X",
            fmtstring(val),
            ficon4bpp.len,
            ICON_BITMAP_BSIZE
        );
    }

    unsigned char *banner = packer->banner.source.buf;
    memcpy(banner + OFS_BANNER_ICON_BITMAP, ficon4bpp.s, ficon4bpp.len);
    free(ficon4bpp.s);

    if (packer->verbose) {
        fprintf(
            stderr,
            "rompacker:configuration:banner: loaded “%.*s” as the icon bitmap\n",
            fmtstring(val)
        );
    }

    return configok;
}

static cfgresult cfg_banner_iconpal(rompacker *packer, string val, long line)
{
    string ficonpal = floads(val);
    if (ficonpal.len < 0) configerr("could not open icon palette file “%.*s”", fmtstring(val));
    if (ficonpal.len > (long)ICON_PALETTE_BSIZE) {
        configerr(
            "icon palette file “%.*s” size 0x%08lX exceeds maximum size 0x%04X",
            fmtstring(val),
            ficonpal.len,
            ICON_PALETTE_BSIZE
        );
    }

    unsigned char *banner = packer->banner.source.buf;
    memcpy(banner + OFS_BANNER_ICON_PALETTE, ficonpal.s, ficonpal.len);
    free(ficonpal.s);

    if (packer->verbose) {
        fprintf(
            stderr,
            "rompacker:configuration:banner: loaded “%.*s” as the icon palette\n",
            fmtstring(val)
        );
    }

    return configok;
}

enum {
    E_utf8_invalidprefix = -1,
    E_utf8_surrogatehalf = -2,
    E_utf8_outofrange    = -3,
};

static inline long dcont(unsigned char byte, int shift)
{
    return (byte & 0x3F) << shift;
}

static inline void *utf8dec(void *s, long *c)
{
    unsigned char *buf = s;
    unsigned char *next;

    if (buf[0] < 0x80) {
        *c   = buf[0];
        next = buf + 1;
    } else if ((buf[0] & 0xE0) == 0xC0) {
        *c   = ((buf[0] & 0x1F) << 6) | dcont(buf[1], 0);
        next = buf + 2;
    } else if ((buf[0] & 0xF0) == 0xE0) {
        *c   = ((buf[0] & 0x0F) << 12) | dcont(buf[1], 6) | dcont(buf[2], 0);
        next = buf + 3;
    } else if ((buf[0] & 0xF8) == 0xF0 && buf[0] <= 0xF4) {
        *c   = E_utf8_outofrange;
        next = buf + 1;
    } else {
        *c   = E_utf8_invalidprefix;
        next = buf + 1;
    }

    if (*c >= 0xD800 && *c <= 0xDFFF) *c = E_utf8_surrogatehalf;
    return next;
}

#define puttitlechar(__banner, __packer, __c)                                              \
    {                                                                                      \
        putlehalf((__banner) + OFS_BANNER_TITLE_JP + (__packer)->endbannertitle, __c);     \
        putlehalf((__banner) + OFS_BANNER_TITLE_EN + (__packer)->endbannertitle, __c);     \
        putlehalf((__banner) + OFS_BANNER_TITLE_FR + (__packer)->endbannertitle, __c);     \
        putlehalf((__banner) + OFS_BANNER_TITLE_DE + (__packer)->endbannertitle, __c);     \
        putlehalf((__banner) + OFS_BANNER_TITLE_IT + (__packer)->endbannertitle, __c);     \
        putlehalf((__banner) + OFS_BANNER_TITLE_ES + (__packer)->endbannertitle, __c);     \
        if ((__packer)->bannerver > 1)                                                     \
            putlehalf((__banner) + OFS_BANNER_TITLE_CN + (__packer)->endbannertitle, __c); \
        if ((__packer)->bannerver > 2)                                                     \
            putlehalf((__banner) + OFS_BANNER_TITLE_KR + (__packer)->endbannertitle, __c); \
                                                                                           \
        (__packer)->endbannertitle += 2;                                                   \
    }

static inline cfgresult cfg_banner_titlepart(rompacker *packer, string val, long line)
{
    unsigned char *curr   = val.s;
    unsigned char *banner = packer->banner.source.buf;
    while (curr - val.s < val.len && packer->endbannertitle < BANNER_TITLE_BSIZE) {
        long decoded = 0;

        unsigned char *next = utf8dec(curr, &decoded);
        switch (decoded) {
        case -1: configerr("expected a valid UTF-8 encoding, but found “%.*s”", fmtstring(val));

        case -2: configerr("unexpected UTF-8 surrogate pair in “%.*s”", fmtstring(val));

        case -3:
            configerr(
                "expected Basic Multilingual Plane Unicode, but found “%.*s”",
                fmtstring(val)
            );

        default: break;
        }

        puttitlechar(banner, packer, decoded);
        curr = next;
    }

    if (packer->endbannertitle >= BANNER_TITLE_BSIZE && curr - val.s < val.len) {
        configerr(
            "total banner title length is greater than the maximum allowable size 0x%04X",
            BANNER_TITLE_BSIZE
        );
    }

    return configok;
}

static cfgresult cfg_banner_title(rompacker *packer, string val, long line)
{
    if (packer->endbannertitle != 0) {
        configerr("attempted to set title after setting some other value");
    }

    cfgresult result = cfg_banner_titlepart(packer, val, line);
    if (result.code != E_config_none) return result;

    if (packer->verbose) {
        fprintf(stderr, "rompacker:configuration:banner: set title to “%.*s”\n", fmtstring(val));
    }

    return configok;
}

static cfgresult cfg_banner_subtitle(rompacker *packer, string val, long line)
{
    if (packer->endbannertitle == 0) {
        configerr("attempted to set subtitle before setting primary title");
    }

    if (packer->hasbannerdev) configerr("attempted to set subtitle after setting developer");

    if (packer->hasbannersub) configerr("attempted to set multiple subtitles");

    unsigned char *banner = packer->banner.source.buf;
    puttitlechar(banner, packer, '\n');

    cfgresult result = cfg_banner_titlepart(packer, val, line);
    if (result.code != E_config_none) return result;

    if (packer->verbose) {
        fprintf(stderr, "rompacker:configuration:banner: set subtitle to “%.*s”\n", fmtstring(val));
    }

    return configok;
}

static cfgresult cfg_banner_developer(rompacker *packer, string val, long line)
{
    if (packer->endbannertitle == 0) {
        configerr("attempted to set developer before setting primary title");
    }

    if (packer->hasbannerdev) configerr("attempted to set multiple developers");

    unsigned char *banner = packer->banner.source.buf;
    puttitlechar(banner, packer, '\n');

    cfgresult result = cfg_banner_titlepart(packer, val, line);
    if (result.code != E_config_none) return result;

    if (packer->verbose) {
        fprintf(
            stderr,
            "rompacker:configuration:banner: set developer to “%.*s”\n",
            fmtstring(val)
        );
    }

    return configok;
}

// clang-format off
static const keyvalueparser kvparsers[] = {
    { .key = string("version"),   .parser = cfg_banner_version   },
    { .key = string("icon4bpp"),  .parser = cfg_banner_icon4bpp  },
    { .key = string("iconpal"),   .parser = cfg_banner_iconpal   },
    { .key = string("title"),     .parser = cfg_banner_title     },
    { .key = string("subtitle"),  .parser = cfg_banner_subtitle  },
    { .key = string("developer"), .parser = cfg_banner_developer },
    { .key = stringZ,             .parser = NULL                 },
};
// clang-format on

cfgresult cfg_banner(string sec, string key, string val, void *user, long line) // NOLINT
{
    (void)sec;
    rompacker *packer = user;

    const keyvalueparser *match = &kvparsers[0];
    for (; match->parser != NULL && !strequ(key, match->key); match++);

    if (!match->parser) configerr("unrecognized banner-section key “%.*s”", fmtstring(key));
    if (!packer->banner.source.buf && !strequ(string("version"), match->key)) {
        configerr("attempted to set banner-section value before specifying the version");
    }

    return match->parser(packer, val, line);
}
