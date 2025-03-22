# Copyright 2025 <lhearachel@proton.me>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0

.PHONY: all tidy clean

CFLAGS += -MMD -std=c17 -g3 -O0
CFLAGS += -Wall -Wextra
CFLAGS += -Iinclude
CFLAGS += -include global.h

CFLAGS += -fsanitize=address,undefined,unreachable
CFLAGS += -fsanitize-trap
LDFLAGS += -fsanitize=address,undefined,unreachable
LDFLAGS += -fsanitize-trap

TARGET = ndsmake

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

all: $(TARGET)

tidy:
	$(RM) $(OBJ) $(DEP) $(TARGET)

clean: tidy
	$(RM) compile_commands.json

-include $(DEP)
