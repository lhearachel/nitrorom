#!/usr/bin/env bash

# Usage: Sets up a testing directory using a pokeplatinum project as a base.
function usage() {
    echo "setup-testdir <PATH/TO/TESTDIR> <PATH/TO/POKEPLATINUM/PROJECT>"
}

case "$1" in
-h | --help)
    usage
    exit 0
    ;;
esac

if [[ "$#" -ne 2 ]]; then
    echo "missing positional arguments"
    usage
    exit 1
fi

TESTDIR="$1"
POKEPLATINUM=$(realpath "$2")

mkdir -p "$TESTDIR"
ln -s "$POKEPLATINUM/platinum.us/rom_header_template.sbin" "$TESTDIR/header_template.sbin"
ln -s "$POKEPLATINUM/subprojects/NitroSDK-4.2.30001/components/ichneumon_sub.sbin" "$TESTDIR/sub.sbin"
ln -s "$POKEPLATINUM/subprojects/NitroSDK-4.2.30001/components/ichneumon_sub_defs.sbin" "$TESTDIR/sub_defs.sbin"
ln -s "$POKEPLATINUM/build/platinum.us/icon.nbfc" "$TESTDIR/icon.4bpp"
ln -s "$POKEPLATINUM/build/platinum.us/icon.nbfp" "$TESTDIR/icon.pal"
ln -s "$POKEPLATINUM/build/res" "$TESTDIR/res"

for file in "$POKEPLATINUM"/build/*.sbin; do
    ln -s "$file" "$TESTDIR/$(basename "$file")"
done
