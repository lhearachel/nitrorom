/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"

typedef struct {
    char *p;
    isize len;
} String;

typedef struct {
    const char *p_line;
    int line_len;
    int line_num;
    int line_err_begin;
    int line_err_end;
} ErrorCoord;

static String fload(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "cannot open file\n");
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    isize fsize = ftell(f);
    rewind(f);

    char *dest = malloc(fsize + 1);
    fread(dest, 1, fsize, f);
    fclose(f);

    dest[fsize] = '\0';
    return (String){
        .p = dest,
        .len = fsize,
    };
}

static void report_err(const char *filename, const char *source, LexResult *result)
{
    const char *p_line = source;
    int line_num = 0;
    int err_pre_len = 0;
    int err_len = 0;
    int err_post_len = 0;

    int i;
    for (i = 0; i < result->err_begin; i++) {
        if (source[i] == '\n') {
            p_line = source + i + 1;
            line_num++;
            err_pre_len = 0;
        } else {
            err_pre_len++;
        }
    }

    do {
        err_len++;
        i++;
    } while (i < result->err_end);

    while (source[i++] != '\n') {
        err_post_len++;
    }

    // bump the error length by 1 character for invalid filepaths, so that we highlight
    // the invalid token at the end.
    err_len += (result->err_type == E_invalid_filepath);
    err_post_len -= (result->err_type == E_invalid_filepath);

    //               BOLD[file:line:char]    BOLD_RED[error:]        error message        BOLD_RED[token]
    fprintf(stderr, "\x1B[1m%s:%d:%d:\x1B[0m \x1B[1;31merror:\x1B[0m %s: \x1B[1;31m“%.*s”\x1B[0m\n",
            filename, line_num, err_pre_len + 1,
            error_messages[result->err_type],
            err_len, source + result->err_begin);

    //               line_num | BOLD[line_to_err]BOLD_RED[err]BOLD[line_from_err]
    fprintf(stderr, "%5d | \x1B[1m%.*s\x1B[1;31m%.*s\x1B[0m\x1B[1m%.*s\x1B[0m\n",
            line_num,
            err_pre_len, p_line,
            err_len, p_line + err_pre_len,
            err_post_len, p_line + err_pre_len + err_len);

    // red squiggle under the error span
    fprintf(stderr, "      | ");
    for (int i = 0; i < err_pre_len; i++) {
        fputc(' ', stderr);
    }
    fputs("\x1B[1;31m", stderr);
    for (int i = 0; i < err_len - 1; i++) {
        fputc('~', stderr);
    }
    fprintf(stderr, "^\x1B[0m\n");
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        fprintf(stderr, "no file given\n");
    }

    const char *filename = argv[1];
    String source = fload(filename);

    LexResult lexed = lex(source.p, source.len);
    if (!lexed.ok) {
        report_err(filename, source.p, &lexed);
        exit(EXIT_FAILURE);
    }

    free(lexed.tokens);
}
