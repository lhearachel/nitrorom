# A Look at the Original NitroSDK `makerom` Implementation

The original tooling packages that shipped with NitroSDK for the Nintendo DS
bundled an executable called `makerom`. This program was, by its own
description, a tool for creating "NITRO application ROM images... based on the
entries in [a] `ROM Spec` file. This document aims to provide an overview of
this program and provide insight into how `ndsmake` is designed.

## Program Execution

`makerom` provided three methods of execution, controlled via the presence of
specific program-switches:

1. "List" mode; this mode was invoked by specifying the `-l` switch. It would
   parse an input `ROM Spec` file, compute the resulting layout of the ROM
   image, and dump that layout to a file.
2. "ROM" mode; this mode was invoked by specifying the `-r` switch. It accepted
   a file of ROM image contents -- which should have been computed and dumped
   by a previous execution in "list" mode -- and wrote the resulting ROM image
   to disk.
3. "Union" mode; this mode was invoked by specifying _neither_ the `-l` switch
   nor the `-r` switch. It would act as a union of these two modes, yet still
   wrote the artifacts from "list" mode to disk as an intermediary.

The program provides support for a few additional flags, some of which are
specific to either List mode or ROM mode. All flags are available in Union mode.
From the program's own help-text:

```bash
makerom [-d] [-DNAME=VALUE...] [-MDEFINES_FILE] [-F] [-A] [-VFMT_VER] [-WWARNING_TYPE] SPECFILE [ROMFILE] [LISTFILE]
makerom [-d] [-DNAME=VALUE...] [-MDEFINES_FILE] [-VFMT_VER] -l SPECFILE [LISTFILE]
makerom [-d] [-F] [-A] [-VFMT_VER] -r LISTFILE [ROMFILE]
```

`SPECFILE` is taken to be a specification using a domain-specific language.
Details on the format are elaborated in [ROM Spec File
Format](#rom-spec-file-format).

`LISTFILE` is taken as a list-file for input and/or output. If it is not
specified, then the program will take the name of `SPECFILE` and replace its
extension with `.nlf` for output.

`ROMFILE` is taken as a rom-file for output. If it is not specified, then the
program will take the name of `SPECFILE`/`LISTFILE` (depending on the execution
mode) and replace its extension with `.rom` for output.

From inspection of the binary, the program options act like so:

| Option |       Argument | Description                                                                                                      |
| ------ | -------------: | ---------------------------------------------------------------------------------------------------------------- |
| `-d`   |            N/A | Emit verbose program logs to standard output                                                                     |
| `-D`   |   `NAME=VALUE` | Specify `NAME` as a variable for substitution with `VALUE`                                                       |
| `-M`   | `DEFINES_FILE` | Specify `DEFINES_FILE` to be read as a list of `NAME=VALUE` pairs                                                |
| `-F`   |            N/A | Create a ROM image file, even if it exceeds the value of `RomSize` in `SPECFILE`                                 |
| `-A`   |            N/A | Emit an error and stop execution if adding a digital signature would exceed the value of `RomSize` in `SPECFILE` |
| `-V`   |      `FMT_VER` | Specify the ROM image format version; by default, this is assumed to be 2.                                       |
| `-W`   | `WARNING_TYPE` | Output warnings which might otherwise be suppressed.                                                             |

## ROM Spec File Format

This input file to the program controls the layout of the resulting ROM image.
It is divided into four sections, of which two share a format.

### `Arm9` / `Arm7` Sections

```text
Arm9
{
    Static          main.sbin
    OverlayDefs     main_defs.sbin
    OverlayTable    main_table.sbin
    Nef             main.nef
}

Arm7
{
    Static          sub.sbin
    OverlayDefs     sub_defs.sbin
    OverlayTable    sub_table.sbin
    Nef             sub.nef
}
```

These sections configure the layout of the executables supplied to either of the
DS's co-processors (the `ARM946E-S` and `ARM7TDMI`). The parameters to this
section are as follows:

- `Static`: Specify the path to the always-loaded code binary for this processor.
  For the Arm9 section, the basename of this file-path will also be taken as a
  base for some future files (described later).
- `OverlayDefs`: Specify the path to a linker artifact which defines important
  memory addresses and a list of files to be loaded as overlays for this
  processor. These important memory address are, in-order:
  - The RAM address at which the Static code binary for this processor should be
      loaded.
  - The RAM address for the entry-point to this processor's Static code binary.
  - The total size of this processor's Static code binary, in bytes.
  - The RAM address of a callback routine to be executed when this ROM is used
      in auto-boot mode.
- `OverlayTable`: Specify the path to a linker artifact which defines important
  parameters for the overlays used by this processor. For each overlay, the
  following values are emitted, in-order, each as 4-byte little-endian words:
  - The sequential, enumerated ID of the overlay.
  - The RAM address at which this overlay is to be loaded.
  - The size of the overlay's `.text` section.
  - The size of the overlay's `.bss` section.
  - The RAM address of the start of this file's `.sinit` section. Declared by
      invoking `NitroStaticInit` inside the overlay code, which will specify the
      RAM addresses of the entry-routine and exit-routine.
  - The RAM address of the end of this file's `.sinit` section.
  - The filesystem ID of this overlay. Typically, this will be equivalent to the
      enumerated ID of the overlay, but it need not be.
  - A reserved section. Some games will use this section (e.g., Pokemon Heart
      Gold and Soul Silver) to mark individual overlays as compressed.
- `Nef` / `Elf`: These properties are interchangeable. Specify the path to a
  code binary which contains debug information. Most retail ROMs will not ship
  with this parameter set.

### `Property` Section

```text
Property
{
    RomHeaderTemplate   rom_header_template.sbin
    TitleName           GAME TITLE
    MakerCode           01
    RemasterVersion     0
    RomSpeedType        1TROM
    RomSize             1G
    RomFootPadding      TRUE
    RomHeader           rom_header.sbin
    FileName            main_files.sbin
    BannerFile          banner.bnr
    ForChina            FALSE
    ForKorea            FALSE
}
```

This section defines some incidental data which is used when constructing the
ROM image. Some of these parameters are required to be set by the program, and
it will terminate if they are not set.

- `RomHeaderTemplate`: **Optional.** Specify the template file to be used in the
  output ROM's header section. When not specified, it is taken to have a value
  equal to `rom_header.template.sbin`, which is a static binary that shipped
  alongside the SDK and contained important proprietary values, such as the
  compressed bitmap of the Nintendo logo and the expected CRC of that bitmap.
- `TitleName`: **Optional**. Specify a 12-character ASCII string to be used as
  the output ROM's title, embedded in the header section. Any remainder of the
  12-character allottment would be filled with zeros.
- `MakerCode`: **Optional**. Specify a 2-character ASCII string to be used as
  the output ROM's manufacturer code. Typically, this code would be supplied to
  a developer as part of the licensing agreement with Nintendo.
- `RemasterVersion`: **Optional**. Specify a number for the remaster-version of
  the output ROM. Numbers from 0 to 255 are accepted in any of octal, decimal,
  or hexadecimal notation.
- `RomSpeedType`: **Soft-Required**. DS cartridges of size up to `0x04000000`
  bytes could specify this field as either `MROM` (Mask ROM) or `1TROM`
  (one-time programmable ROM). Cartridges larger than this size could only
  specify `1TROM`. The latter of these used a slower data-serialization bus.
  `UNDEFINED` is also accepted as a value, but would not have been accepted by
  Nintendo for final licensing and publication.
- `RomSize`: **Optional**. Specify the ROM size in bits, using one of a subset
  of strings:

    | Parameter Value | ROM Capacity (bytes) | Approximate Capacity |
    | --------------- | -------------------- | -------------------- |
    | `64M`           | `0x00800000`         | ~8.4 megabytes       |
    | `128M`          | `0x01000000`         | ~16.8 megabytes      |
    | `256M`          | `0x02000000`         | ~33.5 megabytes      |
    | `512M`          | `0x04000000`         | ~67.1 megabytes      |
    | `1G`            | `0x08000000`         | ~134.2 megabytes     |
    | `2G`            | `0x10000000`         | ~268.4 megabytes     |
    | `4G`            | `0x20000000`         | ~536.9 megabytes     |
    | `8G`            | `0x40000000`         | ~1.08 gigabytes      |
    | `16G`           | `0x80000000`         | ~2.15 gigabytes      |

- `RomFootPadding`: **Optional**. Specify `TRUE` to have unused capacity in the
  output ROM to be filled with padding values up to the requested capacity. The
  padding value used is a single byte and specified in the `RomSpec` section.
  By default, this parameter is presumed to be `FALSE`.
- `RomHeader`: **Optional**. Specify the path to a pre-compiled ROM header.
  While accepted, such use may have been atypical at the time, as the SDK
  documentation for this tool strongly recommended using the pre-packaged
  template shipped alongside it and modifying properties of the header using
  this section.
- `FileName`: **Optional**. Specify the path to a pre-compiled filename table to
  be shipped inside the output ROM. While accepted, such use may have been
  atypical at the time, as the tool itself would compute this filename table
  from the `RomSpec` section by default.
- `BannerFile`: **Required**. Specify the path to the banner file for the output
  ROM, which would contain identifying information displayed in the DS
  boot-menu: character and palette data for an icon, the game's title, and the
  game's developer.
- `ForChina` / `ForKorea`: **Optional**. Specify `TRUE` to designate that the
  output ROM should be configured to run on the Chinese / Korean version of the
  Nintendo DS. By default, this parameter is presumed to be `FALSE`.

### `RomSpec` Section

This section _must_ be placed after all other sections. Rather than specify all
possible parameters in an example as with previous sections, the individual
parameters will be described in isolation. This section is processed as a
sequence of _directives_, each of which alters the packing-state of the output
ROM in some fashion.

- `Offset`: Specify an offset value to which the write-cursor should be placed.
  This offset value must be larger than the position of the write-cursor after
  writing all previous files. If it is not, then an error will be emitted, and
  the program will terminate.
- `OffsetMin`: This directive behaves similarly to `Offset`, but raises an error
  in all cases, yet does _not_ terminate the program.
- `Padding`: Specify a padding-byte value to be used when aligning an output ROM
  image or filling it to capacity. If unspecified, then `0xFF` is used.
- `Align`: Align to the next ROM address which is a multiple of a given value.
  Empty space is filled with the value specified by the most recent `Padding`
  directive. This directive only applies until the *next* file image is loaded.
- `AlignAll`: Align to the next ROM address which is a multiple of a given
  value. Empty space is filled with the value specified by the most recent
  `Padding` directive. This directive applies to *all future* file images. If
  both `Align` and `AlignAll` have been specified, then the larger value is
  accepted. If unspecified, then it is assumed to be `0x200`.

> [!NOTE]
> The NitroSDK's card-read interface is limited to 512-byte block-units. This is
> the ultimate reason for the above default of `0x200`; misaligning data would
> result in a performance degradation.

- `Segment`: Specify the data to be placed in a given section of the ROM. A few
  constant-values are accepted:

  - `RomHeader`: The ROM's header.
  - `Static Arm(9|7)`: The primary code binary for the specified processor.
  - `OverlayTable Arm(9|7)`: The overlay table for the specified processor.
  - `Overlays Arm(9|7)`: The overlay code binaries for the specified processor.
  - `FileName`: The filename table (`FNT`) section. Defines the layout of the
      directory structure of the output ROM's virtual filesystem.
  - `FileAlloc`: The file-allocation table (`FAT`) section. Defines the starting
      and ending offset of individual file images within the output ROM's virtual
      filesystem.
  - `Banner`: The banner file for the output ROM.
  - `All`: This special value is only applicable at the start of the ROM, before
      any files have been placed. It denotes that the ROM layout should follow a
      standard format. Typically, this is the value that is, was, and should be
      used, unless an individual developer wanted absolute control over the
      precise layout of the output ROM. This specific value is equivalent to
      specifying the following directives, in-order:

    ```text
    Segment   RomHeader
    Align     512
    Segment   Static Arm9
    OffsetMin 0x8000
    Segment   OverlayTable Arm9
    Segment   Overlays Arm9
    Align     512
    Segment   Static Arm7
    Segment   OverlayTable Arm7
    Segment   Overlays Arm7
    Segment   FileName
    Segment   FileAlloc
    Align     512
    Segment   Banner
    ```

- `HostRoot`: Specify the directory on the build machine where individual files
  can be found. This directive can be invoked multiple times, and successive
  invocations will not affect the search-location for previous files. When
  unspecified, the current working directory is used.
- `Root`: Specify the directory in the ROM's virtual filesystem where individual
  files should be placed. This directive can be invoked multiple times, and
  successive invocations will not affect the output-location for previous files.
  When unspecified, the virtual root directory is used.
- `File`: Specify a file to be stored in the ROM's virtual filesystem. If the
  given value is a directory, then all files within its subdirectories will be
  stored in the ROM image. Files can be rejected from this behavior by a prior
  use of the `Reject` directive. The characters `*` and `?` are accepted as
  multi-character and single-character wildcards, respectively. Multiple files
  can be given at once, separated by spaces.
- `Reject`: Specify a filename or a pattern to be used to reject files from the
  output ROM. More than one pattern can be given at once, separated by spaces.
  As with the `File` directive, the characters `*` and `?` are accepted as
  multi-character and single-character wildcards, respectively.

> [!WARNING]
> The next invocation of the `Reject` command will flush any prior invocations!

Some additional directives such as `Fixed` and `TagType` are accepted, but are
largely not worth-using; the program will automatically detect how to flag
output ROM data as moveable or how to access any output ROM data.

## Variable Substitution

Curiously, the program implements its own internal variable-substitution system.
As described in [Program Execution](#program-execution), variables are assigned
values by using the `-D` and/or `-M` options, and all environment variables are
propagated into the program by default. If a value is assigned by both `-D` and
`-M`, then the value assigned by `-D` takes priority. Otherwise, the last-set
value wins.

```text
RomSpec
{
    Offset  0x00000000
    Segment All
    File    $(FILENAME)
}
```

On parsing this section, the program would substitute the value of the variable
`FILENAME` and accept it as a parameter to the `File` directive. Variables with
file-path values have special modifiers. To illustrate, suppose that we have a
variable value `FILE=/home/myuser/readme.txt`:

| Modifier | Description                                                            | Example                         |
| -------: | ---------------------------------------------------------------------- | ------------------------------- |
|     `:h` | The full path to the file's direct parent, including the trailing `/`. | `$(FILE:h)=/home/myuser/`       |
|     `:t` | The basename of the file.                                              | `$(FILE:t)=readme.txt`          |
|     `:r` | The full path to the file, minus the extension.                        | `$(FILE:r)=/home/myuser/readme` |
|     `:e` | The file extension, without the leading `.`                            | `$(FILE:e)=txt`                 |
