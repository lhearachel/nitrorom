/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * No-code generic vector implementation.
 */

#ifndef VECTOR_H
#define VECTOR_H

typedef struct Vector {
    void *data;
    u32 cap;
    u32 len;
} Vector;

// clang-format off
#define grow(v, T) (                                      \
    (v)->cap *= 2,                                        \
    (v)->data = realloc((v)->data, sizeof(T) * (v)->cap), \
    (v)->data                                             \
)

#define push(v, T) (                     \
    (v)->len >= (v)->cap                 \
        ? (v)->data = grow(v, T),        \
          ((T *)(v)->data) + (v)->len++  \
        : ((T *)(v)->data) + (v)->len++  \
)

#define get(v, T, i) (&((T *)(v)->data)[i])
// clang-format on

#endif // VECTOR_H
