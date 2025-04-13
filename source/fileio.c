#include "fileio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strings.h"

string fload(const char *filename)
{
    FILE *infp = fopen(filename, "rb");
    if (!infp) return string(NULL, -1);

    fseek(infp, 0, SEEK_END);
    long fsize = ftell(infp);
    fseek(infp, 0, SEEK_SET);

    if (fsize < 0) {
        fclose(infp);
        return string(NULL, -1);
    }

    string fcont = string(calloc(fsize, 1), fsize);
    fread(fcont.s, 1, fsize, infp);
    fclose(infp);
    return fcont;
}

string floads(const string filename)
{
    char cfilename[256] = { 0 };
    memcpy(cfilename, filename.s, filename.len <= 255 ? filename.len : 255);
    return fload(cfilename);
}

long fsize(const char *filename)
{
    FILE *infp = fopen(filename, "rb");
    if (!infp) return -1;

    fseek(infp, 0, SEEK_END);
    long fsize = ftell(infp);
    fseek(infp, 0, SEEK_SET);

    fclose(infp);
    return fsize;
}

long fsizes(const string filename)
{
    char cfilename[256] = { 0 };
    memcpy(cfilename, filename.s, filename.len <= 255 ? filename.len : 255);
    return fsize(cfilename);
}
