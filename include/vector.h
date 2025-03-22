#ifndef VECTOR_H
#define VECTOR_H

typedef struct Vector {
    void *data;
    u32 cap;
    u32 len;
} Vector;

// clang-format off
#define grow(v, T) (                         \
    (v)->cap *= 2,                           \
    realloc((v)->data, sizeof(T) * (v)->cap) \
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
