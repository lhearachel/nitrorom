#ifndef MAKEROM_H
#define MAKEROM_H

#include "layout.h"
#include "parser.h"

typedef struct MemberOffsets {
    u32 begin;
    u32 end;
} MemberOffsets;

typedef struct ROMOffsets {
    MemberOffsets arm9;
    MemberOffsets arm7;
    MemberOffsets ovy9;
    MemberOffsets ovy7;
    MemberOffsets fntb;
    MemberOffsets fatb;
    MemberOffsets banner;
    MemberOffsets filesys[]; // includes overlays
} ROMOffsets;

typedef union ROM {
    ROMOffsets *offsets;
    struct {
        byte *data;
        u32 size;
    } buffer;
} ROM;

ROM makerom(ROMSpec *spec, ROMLayout *layout, u16 securecrc, bool dryrun);

#endif // MAKEROM_H
