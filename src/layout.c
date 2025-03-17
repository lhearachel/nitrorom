/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include "layout.h"

#include <stdlib.h>
#include <string.h>

#include "hashmap.h"
#include "io.h"
#include "parser.h"

static ARMDefinitions load_arm_defs(char *filename);
static int compare_files(const void *a, const void *b);
static Vector *build_filesystem(File *sorted, File *unsorted, u32 num_files, u32 file_id);

LayoutResult compute_rom_layout(ROMSpec *romspec)
{
    // Load the ARM9 and ARM7 definitions files first; the overlays that are
    // contained therein are numbered before any filesystem entries are assigned
    // IDs, which we must know in order to build the filename table.
    ROMLayout *layout = calloc(1, sizeof(ROMLayout));
    layout->arm9_defs = load_arm_defs(romspec->arm9.definitions_fpath);
    layout->arm7_defs = load_arm_defs(romspec->arm7.definitions_fpath);

    // Make a sorted copy of the in-ROM filesystem names to both validate that
    // there are no duplicate filenames and to build the filename-table.
    File *sorted_files = malloc(sizeof(File) * romspec->len_files);
    memcpy(sorted_files, romspec->files, sizeof(File) * romspec->len_files);
    qsort(sorted_files, romspec->len_files, sizeof(File), compare_files);

    // Build the subtable entries using the sorted files, updating file IDs in
    // the unsorted files. This is also where we validate if any duplicate
    // target files were specified.
    u32 first_file_id = layout->arm9_defs.num_overlays + layout->arm7_defs.num_overlays;
    layout->filesystem = build_filesystem(sorted_files, romspec->files, romspec->len_files, first_file_id);

    free(sorted_files);
    return (LayoutResult){
        .layout = layout,
        .err_code = 0,
    };
}

// Take a sequence of 4 bytes as a little-endian word.
static inline u32 leword(byte *data)
{
    return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

static ARMDefinitions load_arm_defs(char *filename)
{
    String content = fload(filename);
    byte *defs_raw = (byte *)content.p;

    ARMDefinitions defs = {
        .ram_load_address = leword(defs_raw + 0x00),
        .ram_main_address = leword(defs_raw + 0x04),
        .file_size = leword(defs_raw + 0x08),
        .callback_address = leword(defs_raw + 0x0C),
        .overlay_filenames = null,
        .num_overlays = 0,
    };

    if (content.len > 0x10) {
        // File is small, two passes is not costly and avoids reallocations
        for (u32 i = 0x10; i < content.len; i++) {
            defs.num_overlays += (content.p[i] == '\0');
        }

        defs.overlay_filenames = malloc(sizeof(char *) * defs.num_overlays);
        char *p = content.p + 0x10;
        for (u32 i = 0; i < defs.num_overlays; i++) {
            char *end = strchr(p, '\0');
            defs.overlay_filenames[i] = malloc(end - p);
            strcpy(defs.overlay_filenames[i], p);
            p = end + 1;
        }
    }

    free(content.p);
    return defs;
}

#define lower(c) (((c >= 'A' && c <= 'Z') * (c + ('a' - 'A'))) + ((c < 'A' || c > 'Z') * c))

// compare up to N characters of each string, case-insensitive.
// fewer than N characters are compared if either string is shorter.
static int strncmp_i(const char *a, const char *b, int n)
{
    int i;
    for (i = 0; i < n && *a && *b && lower(*a) == lower(*b); a++, b++, i++)
        ;

    char a_l = lower(*a);
    char b_l = lower(*b);

    //     0 if equal   -1 if a is lesser     1 if b is lesser
    return ((i != n) * (((a_l < b_l) * -1) + ((a_l > b_l) * 1)));
}

// search for a character in the string. unlike strchr, this will return the
// position of the null-terminator if the character is not found.
static const char *strchr_z(const char *s, char c)
{
    const char *p = s;
    for (; *p && *p != c; p++)
        ;

    return p;
}

// search for a character in the string, comparing at-most n characters.
static const char *strchr_n(const char *s, u32 n, char c)
{
    const char *p = s;
    for (; *p && p - s < n && *p != c; p++)
        ;

    return p;
}

// BUG: e.g., `data/sound` out of order
static int compare_files(const void *a, const void *b)
{
    const File *f_a = a;
    const File *f_b = b;

    const char *comp_a = f_a->target_path + 1;
    const char *comp_b = f_b->target_path + 1;

    int result;
    do {
        const char *next_a = strchr_z(comp_a, '/');
        const char *next_b = strchr_z(comp_b, '/');

        result = strncmp_i(comp_a, comp_b, min(next_a - comp_a, next_b - comp_b));

        comp_a = next_a + 1;
        comp_b = next_b + 1;
        result = (*next_a == *next_b) ? result : (*next_a ? 1 : -1);
    } while (*comp_a && *comp_b && result == 0);

    return result;
}

// Split a full path-slice into the lengths of the subpaths in ascending order.
// For example, the following input path:
//
//   application/balloon/graphic
//
// Would store 3 to `num_subpaths` and `subpath_lens` in-order as [11, 19, 27].
static void split_subpaths(char *path, u32 path_len, u32 subpath_lens[8], u32 *num_subpaths)
{
    *num_subpaths = 0;
    const char *slash = strchr_n(path, path_len, '/');
    while (*slash) {
        subpath_lens[*num_subpaths] = slash - path;

        u32 n = path_len - (slash - path);
        slash = n == 0 ? "" : strchr_n(slash + 1, n - 1, '/');
        (*num_subpaths)++;
    }
}

#define INIT_CAP 32

// TODO: Error codes for duplicate filenames
static Vector *build_filesystem(File *sorted, File *unsorted, u32 num_files, u32 file_id)
{
    HashMap *pathmap = hm_new();
    Vector *filesys = malloc(sizeof(Vector));
    filesys->data = calloc(INIT_CAP, sizeof(FilesysDirectory));
    filesys->cap = INIT_CAP;
    filesys->len = 0;

    // The first directory is always root.
    FilesysDirectory *root = push(filesys, FilesysDirectory);
    root->full_name = "";
    root->full_name_len = 0;
    root->first_file_id = file_id;
    root->dir_id = 0;
    root->parent = 0xF001; // implicit directory counter
    root->children = (Vector){
        .data = calloc(INIT_CAP, sizeof(FilesysNode)),
        .cap = INIT_CAP,
        .len = 0,
    };

    u32 subpath_lens[8] = {0};
    for (u32 i = 0; i < num_files; i++) {
        char *target_path = &sorted[i].target_path[1];
        char *final_slash = strrchr(target_path, '/');
        char *basename = final_slash + 1;
        u32 dirname_len = final_slash - target_path;
        u32 basename_len = strlen(basename);
        u32 num_subpaths = 1;
        u32 j = 0;

        // Find the longest matching subtable by full name; default to root.
        split_subpaths(target_path, dirname_len, subpath_lens, &num_subpaths);
        FilesysDirectory *parent = hm_rfind(pathmap, target_path, num_subpaths, subpath_lens, &j);

        char *component = target_path;
        if (!parent) {
            parent = get(filesys, FilesysDirectory, 0); // dodge potential pointer invalidation
        } else {
            component = target_path + subpath_lens[j] + 1;
            j++;
        }

        for (; j < num_subpaths; j++) {
            // Create a child-directory subtable.
            FilesysDirectory *child = push(filesys, FilesysDirectory);
            child->children = (Vector){
                .data = calloc(INIT_CAP, sizeof(FilesysNode)),
                .cap = INIT_CAP,
                .len = 0,
            };
            child->full_name = target_path;
            child->full_name_len = subpath_lens[j];
            child->first_file_id = file_id;
            child->parent = parent->dir_id;
            child->dir_id = get(filesys, FilesysDirectory, 0)->parent;

            // Register the child to its parent.
            FilesysNode *node = push(&parent->children, FilesysNode);
            node->name = component;
            node->name_len = child->full_name_len - (component - child->full_name);
            node->is_subdir = true;
            node->subdir_id = child->dir_id;

            // Register the child in the map and iterate forward.
            hm_set(pathmap, child->full_name, child->full_name_len, child);
            parent = child;
            component = target_path + subpath_lens[j] + 1;
            get(filesys, FilesysDirectory, 0)->parent++;
        }

        // Create the file onto the child subtable.
        FilesysNode *file = push(&parent->children, FilesysNode);
        file->name = basename;
        file->name_len = basename_len;
        file->is_subdir = false;
        unsorted[sorted[i].packing_id].filesys_id = file_id++;
    }

    hm_free(pathmap);
    get(filesys, FilesysDirectory, 0)->parent &= 0x0FFF;
    return filesys;
}

void dlayout(ROMLayout *layout)
{
    for (u32 i = 0; i < layout->arm9_defs.num_overlays; i++) {
        free(layout->arm9_defs.overlay_filenames[i]);
    }

    for (u32 i = 0; i < layout->arm7_defs.num_overlays; i++) {
        free(layout->arm7_defs.overlay_filenames[i]);
    }

    for (u32 i = 0; i < layout->filesystem->len; i++) {
        FilesysDirectory *dir = get(layout->filesystem, FilesysDirectory, i);
        free(dir->children.data);
    }

    free(layout->arm9_defs.overlay_filenames);
    free(layout->arm7_defs.overlay_filenames);
    free(layout->filesystem->data);
    free(layout->filesystem);
}
