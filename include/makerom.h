/*
 * Copyright 2025 <lhearachel@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef MAKEROM_H
#define MAKEROM_H

#include "layout.h"
#include "parser.h"
#include "vector.h"

void makerom(ROMSpec *spec, ROMLayout *layout, byte *fnt, bool dryrun);
byte *makefnt(Vector *filesystem, u32 size);

#endif // MAKEROM_H
