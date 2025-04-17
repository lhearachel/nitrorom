#include "libs/config.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/strings.h"

#define die0(__msg)             \
    {                           \
        fprintf(stderr, __msg); \
        return E_config_user;   \
    }
#define die(__msg, ...)                      \
    {                                        \
        fprintf(stderr, __msg, __VA_ARGS__); \
        return E_config_user;                \
    }

#define cfgresult(__code, __pos, __msg)                  \
    (cfgresult)                                          \
    {                                                    \
        .code = (__code), .msg = (__msg), .pos = (__pos) \
    }

typedef struct test {
    const char       *key;
    const cfgsection *sections;

    int code;
    int npairs;
} test;

typedef struct expect {
    string sec;
    string key;
    string val;
} expect;

typedef struct harness {
    const test *test;
    int         npairs;
} harness;

static cfgresult verify(const expect *expects, string sec, string key, string val, void *user);
static cfgresult verify_simple(string sec, string key, string val, void *user, long line);
static cfgresult verify_comments(string sec, string key, string val, void *user, long line);

// clang-format off
static const expect simple_expectations[] = {
    { .sec = string("Simple Values"), .key = string("key"),              .val = string("value")           },
    { .sec = string("Simple Values"), .key = string("spaces in keys"),   .val = string("allowed")         },
    { .sec = string("Simple Values"), .key = string("spaces in values"), .val = string("allowed as well") },
    { .sec = stringZ },
};

static const expect comments_expectations[] = {
    { .sec = string("You Can Use Comments"), .key = string("spaces around the delimiter"), .val = string("obviously") },
    { .sec = stringZ },
};

static const cfgsection oksections[] = {
    { .section = string("Simple Values"),        .handler = verify_simple   },
    { .section = string("You Can Use Comments"), .handler = verify_comments },
    { .section = stringZ,                        .handler = NULL },
};

static const cfgsection nosections[] = {
    { .section = string("No Keys"), .handler = verify_simple }, // for the nokey test to work
    { .section = stringZ,           .handler = NULL          },
};

static const test tests[] = {
    { .key = "ok",         .sections = oksections, .npairs = 4, .code = E_config_none       },
    { .key = "nokey",      .sections = nosections, .npairs = 0, .code = E_config_nokey      },
    { .key = "nosec",      .sections = nosections, .npairs = 0, .code = E_config_nosec      },
    { .key = "untermsec",  .sections = nosections, .npairs = 0, .code = E_config_untermsec  },
    { .key = "unknownsec", .sections = oksections, .npairs = 0, .code = E_config_unknownsec },
    { 0 },
};
// clang-format on

static cfgresult verify_simple(string sec, string key, string val, void *user, long line)
{
    (void)line;
    return verify(simple_expectations, sec, key, val, user);
}

static cfgresult verify_comments(string sec, string key, string val, void *user, long line)
{
    (void)line;
    return verify(comments_expectations, sec, key, val, user);
}

static cfgresult verify(const expect *expects, string sec, string key, string val, void *user)
{
    if (user == NULL) {
        return cfgresult(E_config_user, key, "expected non-NULL value for user-struct");
    }

    ((harness *)user)->npairs++;

    const expect *match = &expects[0];
    for (; match->sec.len > 0 && !(strequ(sec, match->sec) && strequ(key, match->key)); match++);

    if (match->sec.len == 0) {
        cfgresult res = cfgresult(E_config_user, key, "");
        snprintf(
            res.msg,
            sizeof(res.msg),
            "unrecognized key: %.*s.%.*s",
            fmtstring(sec),
            fmtstring(key)
        );
        return res;
    }

    if (!strequ(val, match->val)) {
        cfgresult res = cfgresult(E_config_user, key, "");
        snprintf(
            res.msg,
            sizeof(res.msg),
            "expected %s.%s = %s, but got %.*s\n",
            match->sec.s,
            match->key.s,
            match->val.s,
            fmtstring(val)
        );
        return res;
    }

    return cfgresult(E_config_none, stringZ, "");
}

int main(int argc, const char **argv)
{
    if (argc < 2) die0("missing arguments: <testkey> <testfile>\n");

    const char *testkey = argv[1];
    const test *test    = &tests[0];
    for (; test->key != NULL && strcmp(test->key, testkey) != 0; test++);
    if (test->key == NULL) die("unknown test key: %s\n", testkey);

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

    harness   harness = { .test = test, .npairs = 0 };
    cfgresult result  = cfgparse(content, test->sections, &harness);

    if (result.code != test->code) {
        free(content.s);
        die("unexpected error: %s\n", result.msg);
    }
    if (harness.npairs != test->npairs) {
        free(content.s);
        die("expected %d pairs but found %d\n", test->npairs, harness.npairs);
    }

    free(content.s);
    return EXIT_SUCCESS;
}
