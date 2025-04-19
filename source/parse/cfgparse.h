// SPDX-License-Identifier: MIT

#ifndef CFGPARSE_H
#define CFGPARSE_H

#include <stdio.h> // NOLINT

#include "packer.h"

#include "libs/config.h"
#include "libs/strings.h"

#define configerr(__msg, ...)                                        \
    {                                                                \
        cfgresult __res = { .code = E_config_user, .pos = stringZ }; \
        snprintf(                                                    \
            (__res).msg,                                             \
            sizeof(__res).msg,                                       \
            "rompacker:configuration:%ld: " __msg,                   \
            line,                                                    \
            ##__VA_ARGS__                                            \
        );                                                           \
        return __res;                                                \
    }

#define configok              \
    (cfgresult)               \
    {                         \
        .code = E_config_none \
    }

typedef cfgresult (*valueparser)(rompacker *packer, string val, long line);

typedef struct keyvalueparser {
    string      key;
    valueparser parser;
} keyvalueparser;

typedef struct strkeyval {
    string       smatch;
    unsigned int val;
} strkeyval;

#endif // CFGPARSE_H
