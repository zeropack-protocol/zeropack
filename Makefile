CC ?= clang
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude

SRCS = src/core.c src/pool.c src/types.c
OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))
LIB  = build/libzeropack.a

EXAMPLE_BIN = build/example_minimal

.PHONY: all clean example

all: $(LIB)

build:
	mkdir -p build

$(LIB): build $(OBJS)
	$(AR) rcs $@ $(OBJS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -Iexamples -c $< -o $@

example: $(LIB)
	$(CC) $(CFLAGS) -Iexamples examples/minimal.c $(LIB) -o $(EXAMPLE_BIN)
	@printf "\n--- Running Minimal Example ---\n"
	@./$(EXAMPLE_BIN)

clean:
	rm -rf build
