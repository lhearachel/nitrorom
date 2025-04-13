#include "sheets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strings.h"

#define die0(__msg)             \
    {                           \
        fprintf(stderr, __msg); \
        return E_sheets_user;   \
    }
#define die(__msg, ...)                      \
    {                                        \
        fprintf(stderr, __msg, __VA_ARGS__); \
        return E_sheets_user;                \
    }

#define sheetsresult(__code, __pos, __msg)               \
    (sheetsresult)                                       \
    {                                                    \
        .code = (__code), .msg = (__msg), .pos = (__pos) \
    }

typedef struct expect {
    const char         *testkey;
    const char         *inputf;
    const unsigned long nrecords;
    const sheetsrecord *records;
} expect;

static const expect expectations[];

sheetsresult verify(sheetsrecord *record, void *user, int line)
{
    expect *expects = user;
    if (record == NULL) {
        return sheetsresult(E_sheets_user, stringZ, "expected a record, but got NULL");
    }
    if (expects == NULL) {
        return sheetsresult(E_sheets_user, stringZ, "expected a user struct, but got NULL");
    }
    if ((unsigned long)line > expects->nrecords) {
        sheetsresult res = sheetsresult(E_sheets_user, stringZ, "");
        snprintf(
            res.msg,
            sizeof(res.msg),
            "expected at most %ld records, but got %d",
            expects->nrecords,
            line
        );
        return res;
    }

    const sheetsrecord *expectrec = &expects->records[line - 1];
    if (record->enclosed != expectrec->enclosed) {
        sheetsresult res = sheetsresult(E_sheets_user, stringZ, "");
        snprintf(
            res.msg,
            sizeof(res.msg),
            "expected enclosed = 0x%016lX, but got 0x%016lX",
            expectrec->enclosed,
            record->enclosed
        );
        return res;
    }

    for (unsigned long i = 0; i < record->nfields; i++) {
        if (!strequ(record->fields[i], expectrec->fields[i])) {
            sheetsresult res = sheetsresult(E_sheets_user, stringZ, "");
            snprintf(
                res.msg,
                sizeof(res.msg),
                "expected field %ld to be %.*s, but found %s\n",
                i,
                (int)record->fields[i].len,
                record->fields[i].s,
                expectrec->fields[i].s
            );
            return res;
        }
    }

    return sheetsresult(E_sheets_none, stringZ, "");
}

int main(int argc, const char **argv)
{
    if (argc < 2) die0("missing arguments: <testkey> <testfile>\n");

    const char *testkey = argv[1];
    expect     *expects = (expect *)&expectations[0];
    for (; expects->testkey != NULL && strcmp(expects->testkey, testkey) != 0; expects++);
    if (expects->testkey == NULL) die("unknown test key: %s\n", testkey);

    FILE *testf = fopen(argv[2], "rb");
    if (!testf) die("test content file “%s” does not exist\n", argv[2]);

    fseek(testf, 0, SEEK_END);
    long fsize = ftell(testf);
    fseek(testf, 0, SEEK_SET);
    if (fsize < 0) {
        fclose(testf);
        die("could not determine size of test file “%s”\n", argv[2]);
    }

    string content = string(malloc(fsize + 1), fsize);
    fread(content.s, 1, content.len, testf);
    fclose(testf);
    content.s[content.len] = '\0';

    sheetsresult result = csvparse(content, verify, verify, expects);

    if (result.code != E_sheets_none) {
        fprintf(stderr, "error: %s\n", result.msg);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

// clang-format off
static const sheetsrecord onerow[] = {
    {
        .fields   = { string("i"), string("am"), string("a"), string("header"), string("row") },
        .nfields  = 5,
        .enclosed = 0,
    },
};

static const sheetsrecord tworows[] = {
    {
        .fields   = { string("i"), string("am"), string("a"), string("header"), string("row") },
        .nfields  = 5,
        .enclosed = 0,
    },
    {
        .fields   = { string("this"), string("is"), string("another"), string("record"), string("mhm!") },
        .nfields  = 5,
        .enclosed = 0,
    },
};

static const sheetsrecord enclosed[] = {
    {
        .fields   = { string("i"), string("am"), string("a"), string("header"), string("row") },
        .nfields  = 5,
        .enclosed = (1 << 1) | (1 << 4),
    },
    {
        .fields   = { string("\"\"i\"\""), string("have"), string("several"), string("\"\"quoted\"\""), string("fields") },
        .nfields  = 5,
        .enclosed = (1 << 0) | (1 << 2) | (1 << 3),
    },
};
// clang-format on

static const expect expectations[] = {
    { .testkey = "onerow", .nrecords = 1, .records = onerow },
    { .testkey = "tworows", .nrecords = 2, .records = tworows },
    { .testkey = "enclosed", .nrecords = 2, .records = enclosed },
    { 0 },
};
