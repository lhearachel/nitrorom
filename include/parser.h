#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

#define LEN_TITLE  12
#define LEN_SERIAL 4
#define LEN_MAKER  2

typedef struct {
    char title[LEN_TITLE];
    char serial[LEN_SERIAL];
    char maker[LEN_MAKER];
    u8 revision;
    u8 pad_to_end;
    u16 rom_type;
    u32 capacity;
    char *header_fpath;
    char *banner_fpath;
} Properties;

typedef struct {
    char *code_binary_fpath;
    char *definitions_fpath;
    char *overlay_table_fpath;
} ARM;

typedef struct {
    char *source_path;
    char *target_path;
    u16 filesys_id : 12;
    u16 is_dir : 4;
    u16 packing_id;
} File;

typedef struct {
    Properties properties;
    ARM arm9;
    ARM arm7;

    File *files;
    u32 len_files;
    u32 cap_files;
} ROMSpec;

typedef struct {
    bool ok;
    union {
        ROMSpec *spec;
        struct {
            u8 err_type;
            int err_idx;
        };
    };
} ParseResult;

ParseResult parse(Token *tokens, int num_tokens, const char *source);
void dspec(ROMSpec *spec);

extern const char *parse_error_messages[];

#endif // PARSER_H
