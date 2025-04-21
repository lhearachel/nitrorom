# NitroROM

Interface with Nintendo DS ROM images.

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Background](#background)
- [Install](#install)
- [Usage](#usage)

## Background

NitroROM is a program for interfacing with Nintendo DS ROM-files in a variety of
ways. Its primary aims are to provide a reusable tool-suite for developers,
modders, hackers, and reverse-engineering researchers. This tool-suite is
sub-divided into a set of commands which act as programs unto themselves; the
top-level executable acts merely as an access-director to these programs.

This project originally began as a packaging utility for building ROM-files with
a similar strategy as that used by the original SDK-tooling. This implementation
is thus able to construct binary-matches of retail ROM-files for decompilation
projects and modders making patches of their outputs using a delta-encoding
format, e.g., [`.xdelta`][gh-xdelta] or [`.bps`][gh-bps-spec].

[gh-xdelta]: https://github.com/jmacd/xdelta
[gh-bps-spec]: https://github.com/blakesmith/rombp/blob/master/docs/bps_spec.md

## Install

This project does not yet have a release distribution for end-users, as it is
still in an alpha-stage where many breaking-changes are anticipated.

Developers and early-adopters can build the project from source:

1. If you have not already, install `meson` using either [their official
   instructions][getting-meson] or [your package manager][repology-meson].
2. If you have not already, [install `libpng`][repology-libpng].

> [!WARNING]
> Developers working on MSYS2 will need to compile `libpng` from source;
> after downloading a source archive:
>
> ```bash
> tar xf libpng-<version>.tar.xz
> cd libpng-<version>
> ./configure --prefix=/usr
> make check
> make install
> ```

3. Clone the repository.
4. Configure the project's build as you like using Meson:

    ```sh
    # Use -O0 with debug symbols
    meson setup --buildtype debug build

    # Use -O2 with debug symbols
    meson setup --buildtype debugoptimized build

    # Use -O3 and strip debug symbols
    meson setup --buildtype release build

    # Link the project against Address Sanitizer for richer crash reports
    meson setup -Db_sanitize=address --buildtype debug build
    ```

5. Use Meson to invoke the build:

    ```sh
    meson compile -C build
    ```

6. Verify that you have successfully built the executable by invoking it with no
   arguments, which will display the program's help text:

    ```sh
    ./build/nitrorom
    ```

Optionally, distributable manual-pages can be generated from the plain-text
files in `docs/`. An alias target is provided for convenience, e.g., if using
the `ninja` backend for `meson`:

```sh
ninja -C build docs
```

This requires setting up the project with the `manuals` option set to `true`,
which must be specified when configuring the build:

```sh
meson setup -Dmanuals=true build
```

The generation targets use [`asciidoctor`][repology-asciidoctor] to convert the
`.adoc` files into manual pages.

[getting-meson]: https://mesonbuild.com/Getting-meson.html
[repology-meson]: https://repology.org/project/meson/versions
[repology-libpng]: https://repology.org/project/libpng/versions
[repology-asciidoctor]: https://repology.org/project/asciidoctor/versions

## Usage

`nitrorom`'s built-in help text provides a basic overview of its usage.

```text
nitrorom - Interface with Nintendo DS ROM images

Usage: nitrorom [OPTIONS] [COMMAND]

Options:
  -h / --help      Display this help-text and exit.
  -v / --version   Display the program's version number and exit.

Commands:
  list             List the components of a Nintendo DS ROM
  pack             Produce a ROM image from source files
```

For details on the usage of individual commands, refer to the associated
documentation in `docs/`. Some example input-files for the `pack` command are
also provided in `examples/`.

> [!NOTE]
> More detailed documentation is a work-in-progress as the project evolves.
