// SPDX-License-Identifier: MIT

#ifndef PACKER_H
#define PACKER_H

#include <stdio.h>

#include "config.h"
#include "sheets.h"
#include "strings.h"
#include "vector.h"

typedef struct source {
    string filename;
    long   size;

    union {
        FILE *hdl;
        void *buf;
    };
} source;

typedef struct rommember {
    source source;
    long   pad;
} rommember;

// We don't maintain file-handles for filesystem members as the upper-bound of filesystem members
// supported by the DS is quite large (61440).
typedef struct romfile {
    string source;
    string target;
    long   size;
    long   pad;
} romfile;

typedef struct rompacker {
    unsigned int packing : 1; // if 0, do not accept further input
    unsigned int verbose : 1; // if 1, emit verbose logs during packing

    // basic sanity-checks for setting banner components
    unsigned int hasbannerver   : 1; // DSi banners are larger, so need a version to properly alloc
    unsigned int hasbannertitle : 1;
    unsigned int hasbannersub   : 1;
    unsigned int hasbannerdev   : 1;

    rommember header;  // intermediate (optional template)
    rommember arm9;    // from disk (required)
    rommember ovt9;    // from disk (optional)
    vector    ovy9;    // T = rommember
    rommember arm7;    // from disk (required)
    rommember ovt7;    // from disk (optional)
    vector    ovy7;    // T = rommember
    rommember fntb;    // intermediate; computed by rompacker_seal
    rommember fatb;    // intermediate; computed by rompacker_seal
    rommember banner;  // intermediate
    vector    filesys; // T = romfile
} rompacker;

enum dumperr {
    E_dump_ok = 0,
    E_dump_packing,
    E_dump_nullfile,
};

// Handlers for expected sections of the packer's config file
cfgresult cfg_header(string sec, string key, string val, void *packer, long line);
cfgresult cfg_rom(string sec, string key, string val, void *packer, long line);
cfgresult cfg_banner(string sec, string key, string val, void *packer, long line);
cfgresult cfg_arm9(string sec, string key, string val, void *packer, long line);
cfgresult cfg_arm7(string sec, string key, string val, void *packer, long line);

// Handler for filesystem CSV parser
sheetsresult csv_addfile(sheetsrecord *record, void *packer, int line);

rompacker   *rompacker_new(unsigned int verbose);
void         rompacker_del(rompacker *packer);
void         rompacker_seal(rompacker *packer);
enum dumperr rompacker_dump(rompacker *packer, FILE *stream);

#endif // PACKER_H
