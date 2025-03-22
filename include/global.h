#ifndef GLOBAL_H
#define GLOBAL_H

#include <stddef.h>
#include <stdint.h>

// clang-format off

// Short-hand for numeric types.
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
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

#define min(a, b) ((((a) <= (b)) * (a)) + (((a) > (b)) * (b)))

#define unused(v) (void)(v)
// clang-format on

typedef struct {
    char *p;
    isize len;
} String;

#endif // GLOBAL_H
