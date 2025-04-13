// SPDX-License-Identifier: MIT

/*
 * fileio - Wrapper functions for file-based I/O.
 * Copyright (C) 2025  <lhearachel@proton.me>
 */

#ifndef FILEIO_H
#define FILEIO_H

#include "strings.h"

/*
 * Load the contents of a file into memory.
 */
string fload(const char *filename);

/*
 * Load the contents of a file into memory. `fload`-wrapper for `string` filenames.
 */
string floads(const string filename);

/*
 * Get the size of a file from disk.
 */
long fsize(const char *filename);

/*
 * Get the size of a file from disk. `fsize`-wrapper for `string` filenames.
 */
long fsizes(const string filename);

#endif // FILEIO_H
