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

#include "errout.h"
#include "io.h"
#include "layout.h"
#include "lexer.h"
#include "parser.h"

int main(int argc, char **argv)
{
    if (argc == 1) {
        fprintf(stderr, "no file given\n");
    }

    const char *filename = argv[1];
    String source = fload(filename);

    LexResult lexed = lex(source.p, source.len);
    if (!lexed.ok) {
        report_lex_err(filename, source.p, &lexed);
        exit(EXIT_FAILURE);
    }

    ParseResult parsed = parse(lexed.tokens, lexed.len, source.p);
    if (!parsed.ok) {
        report_parse_err(filename, source.p, &parsed, &lexed);
        free(lexed.tokens);
        free(source.p);
        exit(EXIT_FAILURE);
    }
    free(lexed.tokens);
    free(source.p);

    LayoutResult laidout = compute_rom_layout(parsed.spec);
    ROMLayout *layout = laidout.layout;

    dspec(parsed.spec);
    dlayout(layout);
}
