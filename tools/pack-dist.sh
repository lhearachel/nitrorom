#!/usr/bin/env bash

set -euo pipefail

rm -rf dist && mkdir -p dist && mkdir -p dist/doc
cp -r include dist/include
cp -r source dist/src
cp docs/*.adoc dist/doc
cp README.md dist/README.md

tar cvzf nitrorom.tar.gz -C dist .
