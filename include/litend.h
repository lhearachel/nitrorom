// SPDX-License-Identifier: MIT

/*
 * litend - Macros for setting little-endian values to a byte buffer.
 * Copyright (C) 2025  <lhearachel@proton.me>
 */

#ifndef LITEND_H
#define LITEND_H

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

#endif // LITEND_H
