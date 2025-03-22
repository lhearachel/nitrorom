#include "errout.h"

#include <stdio.h>

#include "lexer.h"
#include "parser.h"

static ErrorCoord find_err(const char *source, int err_begin, int err_end)
{
    ErrorCoord err = {
        .p_line = source,
        .line_num = 0,
        .pre_len = 0,
        .sub_len = 0,
        .post_len = 0,
    };

    int i;
    for (i = 0; i < err_begin; i++) {
        if (source[i] == '\n') {
            err.p_line = source + i + 1;
            err.line_num++;
            err.pre_len = 0;
        } else {
            err.pre_len++;
        }
    }

    do {
        err.sub_len++;
        i++;
    } while (i < err_end);

    while (source[i++] != '\n') {
        err.post_len++;
    }

    return err;
}

static void report_err(const char *filename, const char *err_msg, const char *err_substr, ErrorCoord err)
{
    //               BOLD[file:line:char]    BOLD_RED[error:]        error message        BOLD_RED[token]
    fprintf(stderr, "\x1B[1m%s:%d:%d:\x1B[0m \x1B[1;31merror:\x1B[0m %s: \x1B[1;31m“%.*s”\x1B[0m\n",
            filename, err.line_num, err.pre_len + 1,
            err_msg,
            err.sub_len, err_substr);

    //               line_num | BOLD[line_to_err]BOLD_RED[err]BOLD[line_from_err]
    fprintf(stderr, "%5d | \x1B[1m%.*s\x1B[1;31m%.*s\x1B[0m\x1B[1m%.*s\x1B[0m\n",
            err.line_num,
            err.pre_len, err.p_line,
            err.sub_len, err.p_line + err.pre_len,
            err.post_len, err.p_line + err.pre_len + err.sub_len);

    // red squiggle under the error span
    fprintf(stderr, "      | ");
    for (int i = 0; i < err.pre_len; i++) {
        fputc(' ', stderr);
    }
    fputs("\x1B[1;31m", stderr);
    for (int i = 0; i < err.sub_len - 1; i++) {
        fputc('~', stderr);
    }
    fprintf(stderr, "^\x1B[0m\n");
}

void report_lex_err(const char *filename, const char *source, LexResult *result)
{
    ErrorCoord err = find_err(source, result->err_begin, result->err_end);

    // bump the error length by 1 character for invalid filepaths, so that we highlight
    // the invalid token at the end.
    err.sub_len += (result->err_type == E_invalid_filepath);
    err.post_len -= (result->err_type == E_invalid_filepath);

    report_err(filename, lex_error_messages[result->err_type], source + result->err_begin, err);
}

void report_parse_err(const char *filename, const char *source, ParseResult *parsed, LexResult *lexed)
{
    Token *bad_token = lexed->tokens + parsed->err_idx;
    ErrorCoord err = find_err(source, bad_token->begin, bad_token->begin + bad_token->len);

    report_err(filename, parse_error_messages[parsed->err_type], source + bad_token->begin, err);
}
