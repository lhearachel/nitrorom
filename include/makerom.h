#ifndef MAKEROM_H
#define MAKEROM_H

#include "layout.h"
#include "parser.h"
#include "vector.h"

void makerom(ROMSpec *spec, ROMLayout *layout, byte *fnt, bool dryrun);
byte *makefnt(Vector *filesystem, u32 size);

#endif // MAKEROM_H
