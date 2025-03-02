/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include "lexer.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
    // terminal states; these will break the state-machine loop to process the
    // computed token.
    S_error = 0,     // some miscellaneous error state; break the outer-loop
    S_error_string,  // string was unterminated before a newline
    S_error_fpath,   // filepath contained an invalid character
    S_done,          // no more characters to process
    S_decimal_num,   // decimal-literals
    S_hexadec_num,   // hexadecimal-literals
    S_string,        // string-literals
    S_filepath,      // filepath-literals
    S_keyword,       // keywords: section-titles, parameters, directives, and constants
    S_begin_section, // begin a section
    S_close_section, // close a section

    // non-terminal states; these will permit the state-machine to continue
    // consuming characters until the token is complete.
    // TODO: do we need to eat through the whole token before processing? this
    // would effectively result in double-iterating all identifiers... but that
    // is probably cheaper than branch-mispredictions if scanning for a
    // terminating character, which would treated as a randomly-bounded loop.
    S_scan,              // whitespace outside a section
    S_scan_section,      // whitespace inside a section
    S_eat_comment,       // eat all characters until the next '\n'
    S_eat_sect_comment,  // eat all characters until the next '\n'
    S_eat_keyword,       // eat alpha-numerics as a token
    S_eat_digit,         // eat decimal-digits as a token
    S_eat_hexadec_digit, // eat hexadecimal-digits as a token
    S_saw_hex_leader,    // eat hexadecimal-digits as a token
    S_saw_zero,          // check the next-character to determine interpretation
    S_eat_string,        // eat all characters until the next '"' or '\n'
    S_eat_filepath,      // eat all characters until the next '"', ' ', or '\n'

    NUM_STATES,
} State;

typedef enum {
    C_misc = 0, // any unclassified character
    C_eof,
    C_space,    // ' ', '\t'
    C_lf,       // '\n'
    C_alphahex, // 'a'-'f', 'A'-'F'
    C_alpha,    // 'g'-'w', 'y'-'z', 'G'-'W', 'Y'-'Z'
    C_x,        // 'x', 'X'
    C_digit,    // '1'-'9'
    C_zero,     // '0'
    C_dot,      // '.'
    C_slash,    // '/'
    C_dquote,   // '"'
    C_lbrace,   // '{'
    C_rbrace,   // '}'
    C_langle,   // '<'
    C_rangle,   // '>'
    C_bslash,   // '\'
    C_question, // '?'
    C_asterisk, // '*'
    C_colon,    // ':'
    C_pipe,     // '|'
    C_hash,     // '#'

    NUM_CLASSES,
} CharClass;

static const u8 char_class[0x80];
static const u8 transition[NUM_STATES][NUM_CLASSES];

#define rewind_token()   (p_source - token_len - 1)
#define is_hex_upper(ch) (ch >= 'A' && ch <= 'F')
#define is_hex_lower(ch) (ch >= 'a' && ch <= 'f')
#define is_dec_digit(ch) (ch >= '0' && ch <= '9')

static TokenType map_keyword(const char *token, int len);

LexResult lex(const char *source, const int source_len)
{
    // initialize a token stream. we make an estimate on the upper-bound for the
    // number of tokens based on the length of the source to optimize out a
    // conditional-branch during the pipeline. if a user figures out how to
    // exceed this, well, good on them; they can file a bug-report. :)
    Token *stream = malloc(sizeof(Token) * (source_len >> 4));
    Token *p_token = stream;

    // initialize the lexer state.
    const char *p_source = source, *p_token_begin = source;
    State state = S_scan;
    bool in_section = false;
    ErrorType err_type = E_unexpected_character;

    while (state > S_done) {
        int token_len = 0;

        // find the next token.
        do {
            int ch = *p_source++;
            int class = char_class[ch];
            state = transition[state][class];
            token_len += (state >= S_eat_keyword && state < NUM_STATES);
        } while (state >= S_scan);

        // interpret the token.
        switch (state) {
        case S_decimal_num:
            p_token_begin = rewind_token();
            p_token->type = T_value_number;
            p_token->n_value = 0;
            for (int i = 0; i < token_len; i++) {
                char ch = *(p_token_begin + i);
                p_token->n_value *= 10;
                p_token->n_value += (ch - '0');
            }
            state = S_scan + in_section;
            break;

        case S_hexadec_num:
            p_token_begin = rewind_token();
            p_token->type = T_value_number;
            p_token->n_value = 0;
            for (int i = 2; i < token_len; i++) { // skip the leading "0x"
                char ch = *(p_token_begin + i);
                p_token->n_value *= 0x10;
                p_token->n_value += (is_dec_digit(ch) * (ch - '0'))
                    + (is_hex_upper(ch) * (ch - 'A' + 10))
                    + (is_hex_lower(ch) * (ch - 'a' + 10));
            }
            state = S_scan + in_section;
            break;

        case S_string:
            p_token_begin = rewind_token() + 1; // first character is '"'
            p_token->type = T_value_string;
            p_token->s_value.begin = p_token_begin - source;
            p_token->s_value.len = token_len - 1; // first character is '"'
            state = S_scan + in_section;
            break;

        case S_filepath:
            p_token_begin = rewind_token();
            p_token->type = T_value_filepath;
            p_token->s_value.begin = p_token_begin - source;
            p_token->s_value.len = token_len;
            state = S_scan + in_section;
            break;

        case S_keyword:
            p_token_begin = rewind_token();
            p_token->type = map_keyword(p_token_begin, token_len);
            p_token->b_value = (p_token->type == T_value_true); // false / empty otherwise

            // revert to S_error if the mapping failed for whatever reason.
            state = ((p_token->type == T_error) * S_error)
                + ((p_token->type != T_error) * (S_scan + in_section));
            err_type = p_token->type == T_error ? E_unknown_keyword : E_unexpected_character;
            break;

        case S_begin_section:
            p_token->type = T_section_begin;
            state = S_scan_section;
            in_section = true;
            break;

        case S_close_section:
            p_token->type = T_section_end;
            state = S_scan;
            in_section = false;
            break;

        case S_error:
        err_cleanup:
            p_token_begin = rewind_token();
            free(stream);
            stream = null;
            break;

        case S_error_string:
            err_type = E_unterminated_string;
            goto err_cleanup;

        case S_error_fpath:
            err_type = E_invalid_filepath;
            goto err_cleanup;

        case S_done:
            break;

        default:
            abort();
        }

        p_token++;
    }

    if (state < S_done) {
        return (LexResult){
            .ok = false,
            .err_type = err_type,
            .err_begin = (p_token_begin - source),
            .err_end = (p_source - source - 1),
        };
    }

    return (LexResult){
        .ok = true,
        .tokens = stream,
        .len = p_token - stream - 1,
    };
}

// convenient for the semantics. yields 0 (T_error) if the token does not match
// the keyword, which plays well with replacing branches with arithmetic.
#define if_keyword_match(s, keyword, token_type) (token_type * (len == lengthof(keyword) && strncmp(s, keyword, len) == 0))

static TokenType map_keyword(const char *token, int len)
{
    switch (*token) {
    case 'A':
        return if_keyword_match(token, "ARM9", T_title_arm9)
            + if_keyword_match(token, "ARM7", T_title_arm7)
            + if_keyword_match(token, "AddFile", T_direc_addfile);

    case 'C':
        return if_keyword_match(token, "CodeBinary", T_param_codebinary);

    case 'D':
        return if_keyword_match(token, "Definitions", T_param_definitions);

    case 'L':
        return if_keyword_match(token, "Layout", T_title_layout);

    case 'M':
        return if_keyword_match(token, "MakerCode", T_param_makercode)
            + if_keyword_match(token, "MROM", T_value_MROM);

    case 'O':
        return if_keyword_match(token, "OverlayTable", T_param_overlaytable);

    case 'P':
        return if_keyword_match(token, "Properties", T_title_properties)
            + if_keyword_match(token, "PadToEnd", T_param_padtoend)
            + if_keyword_match(token, "PROM", T_value_PROM);

    case 'R':
        return if_keyword_match(token, "Revision", T_param_revision)
            + if_keyword_match(token, "ROMType", T_param_romtype)
            + if_keyword_match(token, "ROMCapacity", T_param_romcapacity);

    case 'S':
        return if_keyword_match(token, "Serial", T_param_serial)
            + if_keyword_match(token, "SetTargetPath", T_direc_settargetpath)
            + if_keyword_match(token, "SetSourcePath", T_direc_setsourcepath);

    case 'T':
        return if_keyword_match(token, "Title", T_param_title);

    case 't':
        return if_keyword_match(token, "true", T_value_true);

    case 'f':
        return if_keyword_match(token, "false", T_value_false);

    default:
        return T_error;
    }
}

// basic mapping of valid ASCII characters to character-classes. this compresses
// the number of interpretations that need to be made by interpreting certain
// characters in the same way.
static const u8 char_class[0x80] = {
    ['\0'] = C_eof,
    [' '] = C_space,
    ['\t'] = C_space,
    ['\r'] = C_space, // _should_ always be succeded by LF, so ignore it
    ['\n'] = C_lf,

    ['a'] = C_alphahex,
    ['b'] = C_alphahex,
    ['c'] = C_alphahex,
    ['d'] = C_alphahex,
    ['e'] = C_alphahex,
    ['f'] = C_alphahex,
    ['A'] = C_alphahex,
    ['B'] = C_alphahex,
    ['C'] = C_alphahex,
    ['D'] = C_alphahex,
    ['E'] = C_alphahex,
    ['F'] = C_alphahex,

    ['g'] = C_alpha,
    ['h'] = C_alpha,
    ['i'] = C_alpha,
    ['j'] = C_alpha,
    ['k'] = C_alpha,
    ['l'] = C_alpha,
    ['m'] = C_alpha,
    ['n'] = C_alpha,
    ['o'] = C_alpha,
    ['p'] = C_alpha,
    ['q'] = C_alpha,
    ['r'] = C_alpha,
    ['s'] = C_alpha,
    ['t'] = C_alpha,
    ['u'] = C_alpha,
    ['v'] = C_alpha,
    ['w'] = C_alpha,
    ['y'] = C_alpha,
    ['z'] = C_alpha,
    ['G'] = C_alpha,
    ['H'] = C_alpha,
    ['I'] = C_alpha,
    ['J'] = C_alpha,
    ['K'] = C_alpha,
    ['L'] = C_alpha,
    ['M'] = C_alpha,
    ['N'] = C_alpha,
    ['O'] = C_alpha,
    ['P'] = C_alpha,
    ['Q'] = C_alpha,
    ['R'] = C_alpha,
    ['S'] = C_alpha,
    ['T'] = C_alpha,
    ['U'] = C_alpha,
    ['V'] = C_alpha,
    ['W'] = C_alpha,
    ['Y'] = C_alpha,
    ['Z'] = C_alpha,

    ['x'] = C_x,
    ['X'] = C_x,

    ['0'] = C_zero,
    ['1'] = C_digit,
    ['2'] = C_digit,
    ['3'] = C_digit,
    ['4'] = C_digit,
    ['5'] = C_digit,
    ['6'] = C_digit,
    ['7'] = C_digit,
    ['8'] = C_digit,
    ['9'] = C_digit,

    ['.'] = C_dot,
    ['/'] = C_slash,
    ['"'] = C_dquote,
    ['{'] = C_lbrace,
    ['}'] = C_rbrace,
    ['<'] = C_langle,
    ['>'] = C_rangle,
    ['\\'] = C_bslash,
    ['?'] = C_question,
    ['*'] = C_asterisk,
    [':'] = C_colon,
    ['|'] = C_pipe,
    ['#'] = C_hash,
};

// we only need to keep transitions for non-terminal states; terminal states
// are handled by the switch. this does mean that there are a lot of zeroes
// in the table, but it is what it is. any associations that are not explicitly
// defined will implicitly emit an error.
static const u8 transition[NUM_STATES][NUM_CLASSES] = {
    // only whitespace, comments, section-identifiers, and the opening brace
    // are valid at the top level.
    [S_scan] = {
        [C_eof] = S_done,         // processing complete; implicit error elsewhere
        [C_space] = S_scan,       // eat and discard whitespace
        [C_lf] = S_scan,          // eat and discard whitespace
        [C_hash] = S_eat_comment, // eat a comment

        // start a keyword token.
        [C_alphahex] = S_eat_keyword,
        [C_alpha] = S_eat_keyword,
        [C_x] = S_eat_keyword,

        // '{' starts a section and transfers to S_scan_section.
        [C_lbrace] = S_begin_section,
    },

    // inside of a section, support numeric-, string-, and filepath-literals.
    [S_scan_section] = {
        [C_space] = S_scan_section,    // eat and discard whitespace
        [C_lf] = S_scan_section,       // eat and discard whitespace
        [C_hash] = S_eat_sect_comment, // eat a comment

        // start a keyword token. this may be a parameter keyword or a
        // reserved constant: "MROM", "PROM", "true", "false".
        [C_alphahex] = S_eat_keyword,
        [C_alpha] = S_eat_keyword,
        [C_x] = S_eat_keyword,

        // eat numerals as a numeric-literal. a leading '0' may denote either a
        // decimal- or hexadecimal-literal; any other digit must be in decimal.
        [C_digit] = S_eat_digit,
        [C_zero] = S_saw_zero,

        [C_dquote] = S_eat_string,  // eat a string-literal
        [C_dot] = S_eat_filepath,   // eat a filepath-literal
        [C_slash] = S_eat_filepath, // eat a filepath-literal

        // '}' closes a section and returns to S_scan.
        [C_rbrace] = S_close_section,
    },

    // eat all characters until the next line-feed or EOF.
    [S_eat_comment] = {
        [C_eof] = S_done,
        [C_lf] = S_scan,

        [C_misc] = S_eat_comment,
        [C_space] = S_eat_comment,
        [C_alphahex] = S_eat_comment,
        [C_alpha] = S_eat_comment,
        [C_x] = S_eat_comment,
        [C_digit] = S_eat_comment,
        [C_zero] = S_eat_comment,
        [C_dot] = S_eat_comment,
        [C_slash] = S_eat_comment,
        [C_dquote] = S_eat_comment,
        [C_lbrace] = S_eat_comment,
        [C_rbrace] = S_eat_comment,
        [C_langle] = S_eat_comment,
        [C_rangle] = S_eat_comment,
        [C_bslash] = S_eat_comment,
        [C_question] = S_eat_comment,
        [C_asterisk] = S_eat_comment,
        [C_colon] = S_eat_comment,
        [C_pipe] = S_eat_comment,
        [C_hash] = S_eat_comment,
    },

    // identical to S_eat_comment, except we loop back to S_scan_section
    [S_eat_sect_comment] = {
        [C_eof] = S_done,
        [C_lf] = S_scan_section,

        [C_misc] = S_eat_sect_comment,
        [C_space] = S_eat_sect_comment,
        [C_alphahex] = S_eat_sect_comment,
        [C_alpha] = S_eat_sect_comment,
        [C_x] = S_eat_sect_comment,
        [C_digit] = S_eat_sect_comment,
        [C_zero] = S_eat_sect_comment,
        [C_dot] = S_eat_sect_comment,
        [C_slash] = S_eat_sect_comment,
        [C_dquote] = S_eat_sect_comment,
        [C_lbrace] = S_eat_sect_comment,
        [C_rbrace] = S_eat_sect_comment,
        [C_langle] = S_eat_sect_comment,
        [C_rangle] = S_eat_sect_comment,
        [C_bslash] = S_eat_sect_comment,
        [C_question] = S_eat_sect_comment,
        [C_asterisk] = S_eat_sect_comment,
        [C_colon] = S_eat_sect_comment,
        [C_pipe] = S_eat_sect_comment,
        [C_hash] = S_eat_sect_comment,
    },

    // consume alpha-numerics until a piece of whitespace is encounterd, which
    // terminates the token. any other character is considered an error.
    // NOTE: this cycle of the machine does not maintain any information about
    // whether the keyword comes from the outside or inside of a section. the
    // aspect of flipping from S_keyword to S_scan vs S_scan_section is left to
    // the terminal-state processor.
    [S_eat_keyword] = {
        // STOP; interpret the token
        [C_space] = S_keyword,
        [C_lf] = S_keyword,
        [C_lbrace] = S_keyword, // and also begin the section

        // continue the token.
        [C_alphahex] = S_eat_keyword,
        [C_alpha] = S_eat_keyword,
        [C_x] = S_eat_keyword,
        [C_digit] = S_eat_keyword,
        [C_zero] = S_eat_keyword,
    },

    // must be a decimal-literal. spaces and line-feeds terminate.
    [S_eat_digit] = {
        [C_space] = S_decimal_num, // STOP; interpret the token
        [C_lf] = S_decimal_num,    // STOP; interpret the token
        [C_digit] = S_eat_digit,   // continue the token
    },

    // must be a hexadecimal-literal; note that spaces and line-feeds here imply
    // an error, as the literal is incomplete.
    [S_saw_hex_leader] = {
        // continue the token
        [C_alphahex] = S_eat_hexadec_digit,
        [C_digit] = S_eat_hexadec_digit,
        [C_zero] = S_eat_hexadec_digit,
    },

    // must be a hexadecimal-literal. spaces and line-feeds terminate.
    [S_eat_hexadec_digit] = {
        // STOP; interpret the literal
        [C_space] = S_hexadec_num,
        [C_lf] = S_hexadec_num,

        // continue the token
        [C_alphahex] = S_eat_hexadec_digit,
        [C_digit] = S_eat_hexadec_digit,
        [C_zero] = S_eat_hexadec_digit,
    },

    // must be a numeric-literal of some kind, but the type is ambiguous without
    // the successor. we don't support arithmetic expressions, so we mandate
    // that some kind of whitespace must separate the numeric-literal from its
    // next token
    [S_saw_zero] = {
        // STOP; literal 0
        [C_space] = S_decimal_num,
        [C_lf] = S_decimal_num,

        [C_x] = S_saw_hex_leader, // consume a hexadecimal-literal
        [C_digit] = S_eat_digit,  // consume a decimal-literal
    },

    // must be a string-literal; *any* character here is valid to continue the
    // token, except for '"' -- which terminates it -- or '\n' -- which causes
    // an error.
    [S_eat_string] = {
        [C_lf] = S_error_string, // STOP; unterminated string
        [C_dquote] = S_string,   // STOP; interpret the literal

        // continue the token
        [C_misc] = S_eat_string,
        [C_space] = S_eat_string,
        [C_alphahex] = S_eat_string,
        [C_alpha] = S_eat_string,
        [C_x] = S_eat_string,
        [C_digit] = S_eat_string,
        [C_zero] = S_eat_string,
        [C_dot] = S_eat_string,
        [C_slash] = S_eat_string,
        [C_lbrace] = S_eat_string,
        [C_rbrace] = S_eat_string,
        [C_langle] = S_eat_string,
        [C_rangle] = S_eat_string,
        [C_bslash] = S_eat_string,
        [C_question] = S_eat_string,
        [C_asterisk] = S_eat_string,
        [C_colon] = S_eat_string,
        [C_pipe] = S_eat_string,
        [C_hash] = S_eat_string,
    },

    // must be a filepath-literal; *any* character here is valid to continue
    // the token, except for ' ', or '\n' -- which would terminate it -- or
    // any of the following characters which would cause an error:
    //   - '<' -> reserved on Windows
    //   - '>' -> reserved on Windows
    //   - '"' -> reserved on Windows
    //   - '?' -> reserved on Windows
    //   - '*' -> reserved on Windows
    //   - ':' -> reserved on Windows
    //   - '|' -> reserved on Windows
    //   - '\' -> all filepaths must be Unix-style
    //   - '{' -> reserved by the grammar
    //   - '}' -> reserved by the grammar
    [S_eat_filepath] = {
        // STOP; interpret the literal
        [C_space] = S_filepath,
        [C_lf] = S_filepath,

        // STOP; invalid filepath character
        [C_dquote] = S_error_fpath,
        [C_lbrace] = S_error_fpath,
        [C_rbrace] = S_error_fpath,
        [C_langle] = S_error_fpath,
        [C_rangle] = S_error_fpath,
        [C_bslash] = S_error_fpath,
        [C_question] = S_error_fpath,
        [C_asterisk] = S_error_fpath,
        [C_colon] = S_error_fpath,
        [C_pipe] = S_error_fpath,

        // continue the token
        [C_misc] = S_eat_filepath, // this includes some extra stuff that we probably don't want. YOLO.
        [C_alphahex] = S_eat_filepath,
        [C_alpha] = S_eat_filepath,
        [C_x] = S_eat_filepath,
        [C_digit] = S_eat_filepath,
        [C_zero] = S_eat_filepath,
        [C_dot] = S_eat_filepath,
        [C_slash] = S_eat_filepath,
    },
};

char *error_messages[NUM_ERROR_TYPES] = {
    [E_unexpected_character] = "unexpected character",
    [E_unknown_keyword] = "unknown keyword",
    [E_unterminated_string] = "unterminated string",
    [E_invalid_filepath] = "invalid filepath",
};

#ifndef NDEBUG
char *token_names[NUM_TOKEN_TYPES] = {
    [T_error] = "T_error",
    [T_section_begin] = "T_section_begin",
    [T_section_end] = "T_section_end",
    [T_title_properties] = "T_title_properties",
    [T_title_arm9] = "T_title_arm9",
    [T_title_arm7] = "T_title_arm7",
    [T_title_layout] = "T_title_layout",
    [T_param_title] = "T_param_title",
    [T_param_serial] = "T_param_serial",
    [T_param_makercode] = "T_param_makercode",
    [T_param_revision] = "T_param_revision",
    [T_param_romtype] = "T_param_romtype",
    [T_param_romcapacity] = "T_param_romcapacity",
    [T_param_padtoend] = "T_param_padtoend",
    [T_param_codebinary] = "T_param_codebinary",
    [T_param_definitions] = "T_param_definitions",
    [T_param_overlaytable] = "T_param_overlaytable",
    [T_direc_settargetpath] = "T_direc_settargetpath",
    [T_direc_setsourcepath] = "T_direc_setsourcepath",
    [T_direc_addfile] = "T_direc_addfile",
    [T_value_PROM] = "T_value_PROM",
    [T_value_MROM] = "T_value_MROM",
    [T_value_true] = "T_value_true",
    [T_value_false] = "T_value_false",
    [T_value_number] = "T_value_number",
    [T_value_string] = "T_value_string",
    [T_value_filepath] = "T_value_filepath",
};
#endif // NDEBUG
