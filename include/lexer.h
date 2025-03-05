/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef LEXER_H
#define LEXER_H

typedef enum {
    T_error = 0, // Special error-signifying token.
    T_section_begin,
    T_section_end,

    // Section titles
    T_title_properties,
    T_title_arm9,
    T_title_arm7,
    T_title_layout,

    // Parameters for the Properties section
    T_param_title,
    T_param_serial,
    T_param_makercode,
    T_param_revision,
    T_param_romtype,
    T_param_romcapacity,
    T_param_padtoend,

    // Parameters for the ARM9 / ARM7 sections
    T_param_codebinary,
    T_param_definitions,
    T_param_overlaytable,

    // Directives for the Layout section
    T_direc_settargetpath,
    T_direc_setsourcepath,
    T_direc_addfile,

    // Literals and constants for parameter-values
    T_value_PROM, // constants for T_param_romtype
    T_value_MROM,
    T_value_true, // boolean literals
    T_value_false,
    T_value_number,
    T_value_string,
    T_value_filepath,

    NUM_TOKEN_TYPES,
} TokenType;

typedef enum {
    E_unexpected_character = 0,
    E_unknown_section_keyword,
    E_unknown_section_title,
    E_unterminated_string,
    E_invalid_filepath,

    NUM_ERROR_TYPES,
} ErrorType;

typedef struct {
    TokenType type;
    int begin; // All tokens know their position in the source file for richer
    int len;   // error-reports.

    union {
        bool b_value;
        u32 n_value;
    };
} Token;

typedef struct {
    bool ok;
    union {
        // ok = true
        struct {
            Token *tokens;
            int len;
        };
        // ok = false
        struct {
            ErrorType err_type;
            int err_begin;
            int err_end;
        };
    };
} LexResult;

LexResult lex(const char *source, const int source_len);

extern char *error_messages[NUM_ERROR_TYPES];

#ifndef NDEBUG
extern char *token_names[NUM_TOKEN_TYPES];
#endif // NDEBUG

#endif // LEXER_H
