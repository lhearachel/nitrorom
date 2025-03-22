#include "io.h"

#include <stdio.h>
#include <stdlib.h>

String fload(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "cannot open file “%s”\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    isize fsize = ftell(f);
    rewind(f);

    char *dest = malloc(fsize + 1);
    fread(dest, 1, fsize, f);
    fclose(f);

    dest[fsize] = '\0';
    return (String){
        .p = dest,
        .len = fsize,
    };
}

u32 fsize(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "could not open file “%s”\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    u32 fsize = ftell(f);
    fclose(f);

    return fsize;
}
