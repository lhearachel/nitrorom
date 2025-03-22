#include "makerom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "layout.h"
#include "parser.h"
#include "vector.h"

static inline u32 to512(u32 i)
{
    return (i + 511) & -512;
}

static inline void padto512(byte *rom, u32 end)
{
    u32 padded = to512(end);
    memset(rom + end, 0xFF, padded - end);
}

static inline void putword(byte *p, u32 v)
{
    p[0] = (v & 0x000000FF);
    p[1] = (v & 0x0000FF00) >> 8;
    p[2] = (v & 0x00FF0000) >> 16;
    p[3] = (v & 0xFF000000) >> 24;
}

static inline void puthalf(byte *p, u16 v)
{
    p[0] = (v & 0x00FF);
    p[1] = (v & 0xFF00) >> 8;
}

static u32 nextromsize(u32 size)
{
    for (u64 try = 0x00020000; try <= 0x100000000; try *= 2) {
        if (try >= size) {
            return try;
        }
    }

    // hard failure
    assert(false);
}

#define HEADER_SIZE 0x4000

#define allocsize(size, target_offsets)     \
    {                                       \
        target_offsets.begin = cursor;      \
        target_offsets.end = cursor + size; \
        cursor = to512(target_offsets.end); \
    }
#define allocfile(source_file, target_offsets) allocsize(fsize(source_file), target_offsets)

static inline u32 countfiles(ROMSpec *spec, ROMLayout *layout)
{
    return layout->arm9_defs.num_overlays + layout->arm7_defs.num_overlays + spec->len_files;
}

static ROMOffsets *calcoffsets(ROMSpec *spec, ROMLayout *layout)
{
    u32 numfiles = countfiles(spec, layout);
    ROMOffsets *offsets = calloc(1, sizeof(ROMOffsets) + (numfiles * sizeof(MemberOffsets)));
    u32 cursor = HEADER_SIZE;
    u32 fileid = 0;

    allocfile(spec->arm9.code_binary_fpath, offsets->arm9);
    if (layout->arm9_defs.num_overlays > 0) {
        allocfile(spec->arm9.overlay_table_fpath, offsets->ovy9);
        for (u32 i = 0; i < layout->arm9_defs.num_overlays; i++, fileid++) {
            allocfile(layout->arm9_defs.overlay_filenames[i], offsets->filesys[fileid]);
        }
    }

    allocfile(spec->arm7.code_binary_fpath, offsets->arm7);
    if (layout->arm7_defs.num_overlays > 0) {
        allocfile(spec->arm7.overlay_table_fpath, offsets->ovy7);
        for (u32 i = 0; i < layout->arm7_defs.num_overlays; i++, fileid++) {
            allocfile(layout->arm7_defs.overlay_filenames[i], offsets->filesys[fileid]);
        }
    }

    allocsize(layout->fnt_size, offsets->fntb);
    allocsize(numfiles * sizeof(MemberOffsets), offsets->fatb);
    allocfile(spec->properties.banner_fpath, offsets->banner);
    for (u32 i = 0; i < spec->len_files; i++, fileid++) {
        allocfile(spec->files[i].source_path, offsets->filesys[fileid]);
    }

    return offsets;
}

// This should be safe to skip error checking, because the size-checks handled
// it previously.
static void packfile(byte *rom, MemberOffsets offsets, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    u32 padend = to512(offsets.end);

    fread(rom + offsets.begin, 1, offsets.end - offsets.begin, f);
    memset(rom + offsets.end, 0xFF, padend - offsets.end);
    fclose(f);
}

static void packfntb(byte *rom, ROMLayout *layout, MemberOffsets offsets)
{
    // The FNTB is composed of two sections: Headers and Contents. Each
    // virtual filesystem directory contains one element in each section:
    //
    // struct Header {
    //     u32 offset_to_contents; // originates from the start of the FNT
    //     u16 first_file_id;      // includes files in sub-directories
    //     u16 parent_dir_id;      // for root, this is the count of directories
    // }
    //
    // struct Contents {
    //     u8   name_len : 7;
    //     u8   is_dir   : 1;
    //     char name[name_len];
    //     u16  dir_id; // only filled when is_dir is 1
    // }
    //
    // We know how large the Header section will be based on the total count of
    // directories; to build the full table, we thus keep two write-heads and
    // iteratively build both section-entries for each directory in tandem.

    byte *fntb = rom + offsets.begin;
    byte *p_header = fntb;
    byte *p_contents = fntb + (8 * layout->filesystem->len);

    for (u32 i = 0; i < layout->filesystem->len; i++) {
        FilesysDirectory *directory = get(layout->filesystem, FilesysDirectory, i);
        putword(p_header, p_contents - fntb);
        puthalf(p_header + 4, directory->first_file_id);
        puthalf(p_header + 6, directory->parent);

        for (u32 j = 0; j < directory->children.len; j++) {
            FilesysNode *child = get(&directory->children, FilesysNode, j);

            p_contents[0] = child->name_len | (child->is_subdir << 7);
            memcpy(p_contents + 1, child->name, child->name_len);

            p_contents += 1 + child->name_len;
            if (child->is_subdir) {
                puthalf(p_contents, child->subdir_id);
                p_contents += 2;
            }
        }

        p_header += 8;
        p_contents += 1; // single null-terminator ends the contents entry
    }

    padto512(rom, offsets.end);
}

static void packfatb(byte *rom, ROMSpec *spec, ROMLayout *layout, ROMOffsets *offsets)
{
    // Each FATB entry is a pointer to a filesystem entry with a relative
    // offset to its location in the ROM. These offsets are signed! Thus, the
    // FATB implicitly enforces a maximum ROM capacity of 2 gigabytes.

    u32 numoverlays = layout->arm9_defs.num_overlays + layout->arm7_defs.num_overlays;
    byte *fatb = rom + offsets->fatb.begin;
    byte *p_fatb = fatb;

    for (u32 i = 0; i < numoverlays; i++) {
        putword(p_fatb, offsets->filesys[i].begin);
        putword(p_fatb + 4, offsets->filesys[i].end);
        p_fatb += 8;
    }

    // Filesystem entries are listed in the FATB by their filesystem ID.
    for (u32 i = 0; i < spec->len_files; i++) {
        u16 filesysid = spec->files[i].filesys_id;
        MemberOffsets *member = &offsets->filesys[numoverlays + i];

        p_fatb = fatb + (sizeof(MemberOffsets) * filesysid);
        putword(p_fatb, member->begin);
        putword(p_fatb + 4, member->end);
    }

    padto512(rom, offsets->fatb.end);
}

static u16 calccrc16(byte *data, u32 size, u16 crc)
{
    static u16 table[16] = {
        0x0000,
        0xCC01,
        0xD801,
        0x1400,
        0xF001,
        0x3C00,
        0x2800,
        0xE401,
        0xA001,
        0x6C00,
        0x7800,
        0xB401,
        0x5000,
        0x9C01,
        0x8801,
        0x4400,
    };

    u16 x = 0;
    u16 y;
    u16 bit = 0;
    byte *end = data + size;

    while (data < end) {
        if (bit == 0) {
            x = data[0] | (data[1] << 8);
        }
        y = table[crc & 15];
        crc >>= 4;
        crc ^= y;
        y = table[(x >> bit) & 15];
        crc ^= y;
        bit += 4;
        if (bit == 16) {
            data += 2;
            bit = 0;
        }
    }

    return crc;
}

static void packhead(byte *rom, ROMSpec *spec, ROMLayout *layout, ROMOffsets *offsets, u32 romlen, u32 romcap, u16 securecrc)
{
    packfile(rom, (MemberOffsets){.begin = 0, .end = 0x4000}, spec->properties.header_fpath);

    memcpy(rom + 0x0000, spec->properties.title, LEN_TITLE);
    memcpy(rom + 0x000C, spec->properties.serial, LEN_SERIAL);
    memcpy(rom + 0x0010, spec->properties.maker, LEN_MAKER);

    // chip capacity is stored as a left-shift from the minimum chip capacity
    u32 shiftsize = romcap / 0x00020000;
    u32 shift = 0;
    while (shiftsize >>= 1) {
        shift++;
    }

    rom[0x0014] = shift;
    rom[0x001E] = spec->properties.revision;

    putword(rom + 0x0020, offsets->arm9.begin);
    putword(rom + 0x0024, layout->arm9_defs.ram_main_address);
    putword(rom + 0x0028, layout->arm9_defs.ram_load_address);
    putword(rom + 0x002C, layout->arm9_defs.file_size);
    putword(rom + 0x0050, offsets->ovy9.begin);
    putword(rom + 0x0054, offsets->ovy9.end - offsets->ovy9.begin);
    putword(rom + 0x0070, layout->arm9_defs.callback_address);

    putword(rom + 0x0030, offsets->arm7.begin);
    putword(rom + 0x0034, layout->arm7_defs.ram_main_address);
    putword(rom + 0x0038, layout->arm7_defs.ram_load_address);
    putword(rom + 0x003C, layout->arm7_defs.file_size);
    putword(rom + 0x0058, offsets->ovy7.begin);
    putword(rom + 0x005C, offsets->ovy7.end - offsets->ovy7.begin);
    putword(rom + 0x0074, layout->arm7_defs.callback_address);

    putword(rom + 0x0040, offsets->fntb.begin);
    putword(rom + 0x0044, offsets->fntb.end - offsets->fntb.begin);
    putword(rom + 0x0048, offsets->fatb.begin);
    putword(rom + 0x004C, offsets->fatb.end - offsets->fatb.begin);
    putword(rom + 0x0068, offsets->banner.begin);

    putword(rom + 0x0080, romlen);
    puthalf(rom + 0x006E, spec->properties.rom_type);
    puthalf(rom + 0x0084, 0x4000);     // size of the header
    putword(rom + 0x0088, 0x00004BA0); // ROM address of the static Nitro footer

    if (spec->properties.rom_type == 0x0D7E) {
        putword(rom + 0x0060, 0x00416657); // ROMCTRL for the gamecard bus (normal commands)
        putword(rom + 0x0064, 0x081808F8); // ROMCTRL for the gamecard bus (KEY1 commands)
    } else {
        putword(rom + 0x0060, 0x00586000);
        putword(rom + 0x0064, 0x001808F8);
    }

    puthalf(rom + 0x006C, securecrc);
    puthalf(rom + 0x015E, calccrc16(rom + 0x0000, 0x015E, 0xFFFF));
}

ROM makerom(ROMSpec *spec, ROMLayout *layout, u16 securecrc, bool dryrun)
{
    ROMOffsets *offsets = calcoffsets(spec, layout);
    if (dryrun) {
        return (ROM){.offsets = offsets};
    }

    u32 fileid = 0;
    u32 numfiles = layout->arm9_defs.num_overlays
        + layout->arm7_defs.num_overlays
        + spec->len_files;

    u32 romlen = offsets->filesys[numfiles - 1].end;
    u32 romcap = nextromsize(romlen);
    byte *rom = calloc(romcap, 1);

    packhead(rom, spec, layout, offsets, romlen, romcap, securecrc);
    packfile(rom, offsets->arm9, spec->arm9.code_binary_fpath);
    if (layout->arm9_defs.num_overlays > 0) {
        packfile(rom, offsets->ovy9, spec->arm9.overlay_table_fpath);
        for (u32 i = 0; i < layout->arm9_defs.num_overlays; i++, fileid++) {
            packfile(rom, offsets->filesys[fileid], layout->arm9_defs.overlay_filenames[i]);
        }
    }

    packfile(rom, offsets->arm7, spec->arm7.code_binary_fpath);
    if (layout->arm7_defs.num_overlays > 0) {
        packfile(rom, offsets->ovy7, spec->arm7.overlay_table_fpath);
        for (u32 i = 0; i < layout->arm7_defs.num_overlays; i++, fileid++) {
            packfile(rom, offsets->filesys[fileid], layout->arm7_defs.overlay_filenames[i]);
        }
    }

    packfntb(rom, layout, offsets->fntb);
    packfatb(rom, spec, layout, offsets);
    packfile(rom, offsets->banner, spec->properties.banner_fpath);
    for (u32 i = 0; i < spec->len_files; i++, fileid++) {
        packfile(rom, offsets->filesys[fileid], spec->files[i].source_path);
    }

    if (spec->properties.pad_to_end) {
        memset(rom + romlen, 0xFF, romcap - romlen);
    }

    free(offsets);
    return (ROM){
        .buffer = {
            .data = rom,
            .size = spec->properties.pad_to_end ? romcap : romlen,
        },
    };
}
