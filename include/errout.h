/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef ERROUT_H
#define ERROUT_H

#include "lexer.h"
#include "parser.h"

typedef struct {
    const char *p_line;
    int line_num;
    int pre_len;
    int sub_len;
    int post_len;
} ErrorCoord;

void report_lex_err(const char *filename, const char *source, LexResult *result);
void report_parse_err(const char *filename, const char *source, ParseResult *parsed, LexResult *lexed);

#endif // ERROUT_H
