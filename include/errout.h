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
