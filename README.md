# NitroROM

Create Nintendo DS ROM images from a plain-text specification.

## Table of Contents

<!--toc:start-->
- [Table of Contents](#table-of-contents)
- [Background](#background)
- [Install](#install)
- [Usage](#usage)
<!--toc:end-->

## Background

This project aims to provide a compliant reimplementation of the original
tooling for building ROM images which shipped with the Nintendo DS SDK. The
primary motivator for this work is to construct binary-matches of retail ROM
images for community decompilation projects, but this tool can also of course be
used for compiling homebrew software distributions. Producing binary-matches of
existing ROMs is also potentially useful for modders and hackers, as it will
reduce the size of distributable patch-files produced by delta-encoding formats
like xDelta or BPS.

## Install

This project does not yet have a release distribution for end-users, as it is
still in an alpha-stage where many breaking-changes are anticipated.

Developers and early-adopters can build the project from source:

1. If you have not already, install `meson` using either [their official
   instructions][getting-meson] or [your package manager][repology-meson].
2. Clone the repository.
3. Configure the project's build as you like using Meson:

    ```sh
    # Use -O0 with debug symbols
    meson setup build

    # Use -O2 with debug symbols
    meson setup --buildtype debugoptimized build

    # Link the project against Address Sanitizer for richer crash reports
    meson setup -Db_sanitize=address build
    ```

4. Use Meson to invoke the build:

    ```sh
    meson compile -C build
    ```

5. Verify that you have successfully built the executable by invoking it with no
   arguments, which will display the program's help text:

    ```sh
    ./build/nitrorom
    ```

[getting-meson]: https://mesonbuild.com/Getting-meson.html
[repology-meson]: https://repology.org/project/meson/versions

## Usage

`nitrorom`'s built-in help text provides a basic overview of its usage.

```text
nitrorom - Produce a Nintendo DS ROM from sources

Usage: nitrorom [OPTIONS] <CONFIG.INI> <FILESYS.CSV>

For details on the precise format of CONFIG.INI and FILESYS.CSV, refer to
this program's manual page.

Program Information (must be specified first):
  -h / --help            Display this help-text and exit.
  --version              Display the program's version number and exit.

Options:
  -C / --directory DIR   Change to directory DIR before loading any files.
  -o / --output FILE     Write the output ROM to FILE. Default: “rom.nds”.
  -d / --dry-run         Enable dry-run mode; do not create an output ROM
                         and instead emit computed artifacts: the ROM's
                         header, banner, and filesystem tables.
  -v / --verbose         Enable verbose mode; emit additional program logs
                         during execution to standard-error.
```

As a reference for the format of the configuration files specified above, some
examples are available in `examples/`.

> [!NOTE]
> More detailed documentation is a work-in-progress as the project evolves.
