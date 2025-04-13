// SPDX-License-Identifier: MIT

#ifndef CFGPARSE_H
#define CFGPARSE_H

#include "config.h"
#include "packer.h"
#include "strings.h"

#define configerr(__msg, ...)                                        \
    {                                                                \
        cfgresult __res = { .code = E_config_user, .pos = stringZ }; \
        snprintf(                                                    \
            (__res).msg,                                             \
            sizeof(__res).msg,                                       \
            "rompacker:configuration:%ld: " __msg,                   \
            line,                                                    \
            __VA_ARGS__                                              \
        );                                                           \
        return __res;                                                \
    }

#define configok              \
    (cfgresult)               \
    {                         \
        .code = E_config_none \
    }

#define putleword(__dest, __word)              \
    {                                          \
        (__dest)[0] = (__word) & 0xFF;         \
        (__dest)[1] = ((__word) >> 8) & 0xFF;  \
        (__dest)[2] = ((__word) >> 16) & 0xFF; \
        (__dest)[3] = ((__word) >> 24) & 0xFF; \
    }

#define putlehalf(__dest, __half)             \
    {                                         \
        (__dest)[0] = (__half) & 0xFF;        \
        (__dest)[1] = ((__half) >> 8) & 0xFF; \
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
