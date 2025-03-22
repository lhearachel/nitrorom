#ifndef HASHMAP_H
#define HASHMAP_H

typedef struct Entry {
    char *key;
    int value;

    u32 keylen;
} Entry;

typedef struct HashMap {
    Entry *entries;
    u32 cap;
    u32 len;
} HashMap;

typedef struct EntryIter {
    Entry entry;

    HashMap *_map;
    u32 _index;
} EntryIter;

HashMap *hm_new(void);
void hm_free(HashMap *map);

// Get a value from the map which matches a key of specified length. Returns
// `-1` if no such element exists.
int hm_get(HashMap *map, char *key, u32 keylen);

// Find a value in the map which matches a key of any of a number of specified
// lengths. Returns `-1` if no such element exists. Lengths passed as variadic
// arguments are treated as unsigned 32-bit integers. Key lengths are assessed
// in the order in which they are passed, and the first match wins.
// `match_i` is filled with the index of `keylens` which produced a match, if
// any, or `numlens` if no match was found.
int hm_find(HashMap *map, char *key, int numlens, u32 keylens[], u32 *match_i);

// Find a value in the map which matches a key of any of a number of specified
// lengths. Returns `-1` if no such element exists. Lengths passed as variadic
// arguments are treated as unsigned 32-bit integers. Key lengths are assessed
// in the order in which they are passed; unlike hm_find, the _last_ match wins.
// `match_i` is filled with the index of `keylens` which produced a match, if
// any, or `numlens` if no match was found.
int hm_rfind(HashMap *map, char *key, int numlens, u32 keylens[], u32 *match_i);

// Set a value to the map with an associative key of specified length.
void hm_set(HashMap *map, char *key, u32 keylen, int value);

// Get the next key-value association from an iterator. Returns `false` if all
// elements have been traversed.
bool hm_next(EntryIter *it);

#endif // HASHMAP_H
