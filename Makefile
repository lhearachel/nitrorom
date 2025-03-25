.PHONY: debug release tidy clean

CFLAGS += -MMD -std=c17
CFLAGS += -Wall -Wextra
CFLAGS += -Iinclude
CFLAGS += -include global.h

TARGET = ndsmake

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

release: CFLAGS += -O2
release: CFLAGS += -Wpedantic
release: $(TARGET)

debug: CFLAGS += -g3 -O0
debug: CFLAGS += -fsanitize=address,undefined,unreachable
debug: CFLAGS += -fsanitize-trap
debug: LDFLAGS += -fsanitize=address,undefined,unreachable
debug: LDFLAGS += -fsanitize-trap
debug: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

tidy:
	$(RM) $(OBJ) $(DEP) $(TARGET)

clean: tidy
	$(RM) compile_commands.json

-include $(DEP)
