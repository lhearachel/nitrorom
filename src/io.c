#include "io.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

String fload(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        char buf[256] = {0};
        getcwd(buf, 256);
        fprintf(stderr, "cannot open file “%s” from work directory “%s”: %s\n", filename, buf, strerror(errno));
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
        char buf[256] = {0};
        getcwd(buf, 256);
        fprintf(stderr, "cannot open file “%s” from work directory “%s”: %s\n", filename, buf, strerror(errno));
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    u32 fsize = ftell(f);
    rewind(f);
    fclose(f);

    return fsize;
}
