#include "clip.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct options {
    const char *handle;
    const char *svalue;
    long        flag;
    long        nvalue;
} options;

static int handle(clip *clip, const clipopt *opt, const char *option, void *user)
{
    (void)opt;
    (void)option;

    options *options = user;
    options->handle  = clip->arg;
    return E_clip_none;
}

int main(int argc, const char **argv)
{
    options options = { 0 };

    // clang-format off
    const clipopt clipopts[] = {
        { .longopt = "string", .shortopt = 's', .hasarg = H_reqarg, .starget = &options.svalue },
        { .longopt = "flag",   .shortopt = 'f', .hasarg = H_noarg,  .ntarget = &options.flag   },
        { .longopt = "number", .shortopt = 'n', .hasarg = H_reqarg, .ntarget = &options.nvalue },
        { .longopt = "handle", .shortopt = 'h', .hasarg = H_reqarg, .handler = handle          },
        { 0 },
    };

    const clippos posargs[] = { { 0 } };
    // clang-format on

    clip clip    = clipinit(argv);
    int  cliperr = cliparse(&clip, clipopts, posargs, &options);
    if (cliperr) {
        fprintf(stderr, "test-clip: %s\n", clip.err);
        return EXIT_FAILURE;
    }

    argc -= clip.ind;
    argv += clip.ind;
    while (argc > 0) {
        if (strcmp(*argv, "string") == 0) {
            argc--;
            argv++;
            if (strcmp(*argv, options.svalue) != 0) {
                fprintf(
                    stderr,
                    "test-clip: argument-value mismatch for “string”; e: “%s”, a: “%s”\n",
                    *argv,
                    options.svalue
                );
                return EXIT_FAILURE;
            }
        } else if (strcmp(*argv, "flag") == 0) {
            if (!options.flag) {
                fprintf(stderr, "test-clip: missing assignment for “flag”\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(*argv, "number") == 0) {
            argc--;
            argv++;
            long val = strtol(*argv, NULL, 0);
            if (val != options.nvalue) {
                fprintf(
                    stderr,
                    "test-clip: argument-value mismatch for “number”; e: %ld, a: %ld\n",
                    val,
                    options.nvalue
                );
                return EXIT_FAILURE;
            }
        } else if (strcmp(*argv, "handle") == 0) {
            argc--;
            argv++;
            if (strcmp(*argv, options.handle) != 0) {
                fprintf(
                    stderr,
                    "test-clip: argument-value mismatch for “handle”; e: “%s”, a: “%s”\n",
                    *argv,
                    options.handle
                );
                return EXIT_FAILURE;
            }
        } else {
            fprintf(stderr, "test-clip: unrecognized positional argument “%s”\n", *argv);
            return EXIT_FAILURE;
        }

        argc--;
        argv++;
    }

    return EXIT_SUCCESS;
}
