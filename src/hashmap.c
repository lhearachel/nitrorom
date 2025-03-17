/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

#define INIT_SIZE  256
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME  1099511628211UL

#define HASH_IDX(hash, cap) ((u32)(hash & (u64)((cap) - 1)))

static u64 hashkey(const char *key, u32 keylen);
static void upsert(Entry *entries, u32 cap, u32 *plen, Entry new);
static void expand(HashMap *map);

HashMap *hm_new(void)
{
    HashMap *map = malloc(sizeof(HashMap));
    map->len = 0;
    map->cap = INIT_SIZE;
    map->entries = calloc(INIT_SIZE, sizeof(Entry));

    return map;
}

static u64 hashkey(const char *key, u32 keylen)
{
    u64 hash = FNV_OFFSET;
    for (u32 i = 0; i < keylen; i++) {
        hash ^= (u64)key[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

int hm_get(HashMap *map, char *key, u32 keylen)
{
    u64 hash = hashkey(key, keylen);
    u32 idx = HASH_IDX(hash, map->cap);

    while (map->entries[idx].key != null) {
        if (map->entries[idx].keylen == keylen
            && strncmp(map->entries[idx].key, key, keylen) == 0) {
            return map->entries[idx].value;
        }

        idx = (idx + 1 >= map->cap) ? 0 : idx + 1;
    }

    return -1;
}

int hm_find(HashMap *map, char *key, int numlens, u32 keylens[], u32 *match_i)
{
    int i;
    int found = -1;
    for (i = 0; i < numlens && found < 0; i++) {
        found = hm_get(map, key, keylens[i]);
    }

    *match_i = found >= 0 ? i - 1 : 0;
    return found;
}

int hm_rfind(HashMap *map, char *key, int numlens, u32 keylens[], u32 *match_i)
{
    int i;
    int found = -1;
    for (i = numlens - 1; i >= 0 && found < 0; i--) {
        found = hm_get(map, key, keylens[i]);
    }

    *match_i = found >= 0 ? i + 1 : 0;
    return found;
}

static void upsert(Entry *entries, u32 cap, u32 *plen, Entry new)
{
    u64 hash = hashkey(new.key, new.keylen);
    u32 idx = HASH_IDX(hash, cap);

    while (entries[idx].key != null) {
        if (entries[idx].keylen == new.keylen
            && strncmp(entries[idx].key, new.key, new.keylen) == 0) {
            entries[idx].value = new.value;
            return;
        }

        // Linear probe; if there is a collision on hash-value, then the
        // matching key must be further-ahead in the backing array.
        idx = (idx + 1 >= cap) ? 0 : idx + 1;
    }

    if (plen != null) {
        (*plen)++;
    }

    entries[idx].key = new.key;
    entries[idx].value = new.value;
    entries[idx].keylen = new.keylen;
}

static void expand(HashMap *map)
{
    u32 new_cap = map->cap * 2;
    Entry *new_entries = calloc(new_cap, sizeof(Entry));

    // Re-hash keys to array-indices.
    for (u32 i = 0; i < map->cap; i++) {
        if (map->entries[i].key != null) {
            upsert(new_entries, new_cap, null, map->entries[i]);
        }
    }

    free(map->entries);
    map->entries = new_entries;
    map->cap = new_cap;
}

void hm_set(HashMap *map, char *key, u32 keylen, int value)
{
    assert(value != -1);

    if (map->len * 2 >= map->cap) {
        expand(map);
    }

    Entry new = {
        .key = key,
        .value = value,
        .keylen = keylen,
    };
    upsert(map->entries, map->cap, &map->len, new);
}

bool hm_next(EntryIter *it)
{
    HashMap *map = it->_map;
    while (it->_index < map->cap) {
        u32 i = it->_index;
        it->_index++;
        if (map->entries[i].key != null) {
            it->entry = map->entries[i];
            return true;
        }
    }

    return false;
}

void hm_free(HashMap *map)
{
    free(map->entries);
    free(map);
}
