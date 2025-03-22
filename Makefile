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
