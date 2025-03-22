/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

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

static u32 emit_file_dry(u32 *cursor, u16 filesys_id, const char *source_file, const char *target_file)
{
    u32 size = fsize(source_file);
    u32 start = *cursor;
    printf("%08X,%08X,FF,%04X,%s,%s\n", start, start + size, filesys_id, source_file, target_file ? target_file : "*");
    *cursor = to512(start + size);
    return start + size;
}

static u32 emit_table_dry(u32 *cursor, const byte *table, u32 size)
{
    u32 start = *cursor;
    printf("%08X,%08X,FF,FFFF,%s,*\n", start, start + size, (char *)table);
    *cursor = to512(start + size);
    return start + size;
}

static inline void put_leword(byte *p, u32 v)
{
    p[0] = (v & 0x000000FF);
    p[1] = (v & 0x0000FF00) >> 8;
    p[2] = (v & 0x00FF0000) >> 16;
    p[3] = (v & 0xFF000000) >> 24;
}

static inline void put_lehalf(byte *p, u16 v)
{
    p[0] = (v & 0x00FF);
    p[1] = (v & 0xFF00) >> 8;
}

byte *makefnt(Vector *filesystem, u32 size)
{
    // The FNT is composed of two sections: Headers and Contents. Each directory
    // in the filesystem contains one of each:
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

    byte *fnt = calloc(size, 1);
    byte *p_header = fnt;
    byte *p_contents = fnt + (8 * filesystem->len);

    for (u32 i = 0; i < filesystem->len; i++) { // i == 29
        FilesysDirectory *directory = get(filesystem, FilesysDirectory, i);
        put_leword(p_header, p_contents - fnt);
        put_lehalf(p_header + 4, directory->first_file_id);
        put_lehalf(p_header + 6, directory->parent);

        for (u32 j = 0; j < directory->children.len; j++) {
            FilesysNode *child = get(&directory->children, FilesysNode, j);

            p_contents[0] = child->name_len | (child->is_subdir << 7);
            memcpy(p_contents + 1, child->name, child->name_len);

            p_contents += 1 + child->name_len;
            if (child->is_subdir) {
                put_lehalf(p_contents, child->subdir_id);
                p_contents += 2;
            }
        }

        p_header += 8;
        p_contents += 1; // single null-terminator ends the contents entry
    }

    return fnt;
}

typedef u32 (*EmitFileFunc)(u32 *cursor, u16 filesys_id, const char *source_file, const char *target_file);
typedef u32 (*EmitTableFunc)(u32 *cursor, const byte *table, u32 size);

typedef struct FileAllocation {
    u32 begin;
    u32 end;
} FileAllocation;

static void emit_arm(ARM *arm, ARMDefinitions *defs, u32 *cursor, u32 *file_id, EmitFileFunc emit_file, FileAllocation *fat)
{
    emit_file(cursor, 0xFFFF, arm->code_binary_fpath, null);

    if (defs->num_overlays > 0) {
        emit_file(cursor, 0xFFFF, arm->overlay_table_fpath, null);
        for (u32 i = 0; i < defs->num_overlays; i++, (*file_id)++) {
            FileAllocation *alloc = &fat[*file_id];
            alloc->begin = *cursor;
            alloc->end = emit_file(cursor, *file_id, defs->overlay_filenames[i], null);
        }
    }
}

void makerom(ROMSpec *spec, ROMLayout *layout, byte *fnt, bool dryrun)
{
    u32 cursor = 0, file_id = 0;
    EmitFileFunc emit_file = emit_file_dry;
    EmitTableFunc emit_table = emit_table_dry;

    u32 total_files = layout->arm9_defs.num_overlays + layout->arm7_defs.num_overlays + spec->len_files;
    FileAllocation *fat = calloc(total_files, sizeof(FileAllocation));

    emit_file(&cursor, 0xFFFF, spec->properties.header_fpath, null);
    emit_arm(&spec->arm9, &layout->arm9_defs, &cursor, &file_id, emit_file, fat);
    emit_arm(&spec->arm7, &layout->arm7_defs, &cursor, &file_id, emit_file, fat);

    // Pretend to emit these tables and emit them at the end.
    if (!dryrun) {
        cursor = to512(cursor + layout->fnt_size);
        cursor = to512(cursor + (8 * (file_id + spec->len_files)));
    } else {
        emit_table(&cursor, (byte *)"*FILENAMES*", layout->fnt_size);
        emit_table(&cursor, (byte *)"*FILEALLOC*", total_files * sizeof(FileAllocation));
    }

    emit_file(&cursor, 0xFFFF, spec->properties.banner_fpath, null);
    for (u32 i = 0; i < spec->len_files; i++) {
        File *file = (File *)spec->files + i;
        FileAllocation *alloc = &fat[file_id + i];
        alloc->begin = cursor;
        alloc->end = emit_file(&cursor, file->filesys_id, file->source_path, file->target_path);
    }

    byte *fatb = calloc(total_files * sizeof(FileAllocation), 1);
    byte *fatbp = fatb;
    for (u32 i = 0; i < total_files; i++, fatbp += 8) {
        put_leword(fatbp, fat[i].begin);
        put_leword(fatbp + 4, fat[i].end);
    }

    free(fatb);
    free(fat);
}
