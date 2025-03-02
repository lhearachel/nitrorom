/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef GLOBAL_H
#define GLOBAL_H

#include <stddef.h>
#include <stdint.h>

// clang-format off

// Short-hand for numeric types.
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef ptrdiff_t isize;
typedef size_t    usize;

// Short-hand for some extra nice types. (I hate stdbool.h)
typedef uint8_t   byte;
typedef uint32_t  bool;

// Semantic niceties for common values.
#define null  (void *)0
#define false 0
#define true  1

// Convenience macros.
#define alignof(a)  _Alignof(a)
#define countof(a)  (isize)(sizeof(a) / sizeof(*(a)))
#define lengthof(s) (countof(s) - 1)

#define assert(c)                while (!(c)) __builtin_unreachable()
#define static_assert(c, ...)    _Static_assert(c, ##__VA_ARGS__)
// clang-format on

#endif // GLOBAL_H
