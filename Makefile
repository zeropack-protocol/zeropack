CC ?= clang
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -MMD -MP

SRCS = src/core.c src/tx.c src/rx.c src/handshake.c src/pool.c src/types.c

OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)
LIB  = build/libzeropack.a

EXAMPLE_BIN = build/example_minimal
TEST_BIN    = build/test_runner

.PHONY: all clean example test test-fail

all: $(LIB)

build:
	mkdir -p build

$(LIB): build $(OBJS)
	$(AR) rcs $@ $(OBJS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

test: $(LIB) test-fail
	$(CC) $(CFLAGS) -Itest test/test_main.c $(LIB) -o $(TEST_BIN)
	@$(TEST_BIN)

test-fail:
	@printf "\n=== Running Negative Compilation Tests ===\n"
	@TMP_LOG=$$(mktemp); \
	trap 'rm -f $$TMP_LOG' EXIT; \
	run_case() { \
		flag="$$1"; pat="$$2"; name="$$3"; \
		if $(CC) $(filter-out -MMD -MP,$(CFLAGS)) -Werror -Wno-unused-command-line-argument -D$$flag -c test/test_fail.c -o /dev/null 2>$$TMP_LOG; then \
			echo "  [FAIL] $$name (compilation should have failed!)"; exit 1; \
		elif grep -i -E "$$pat" $$TMP_LOG >/dev/null; then \
			echo "  [PASS] $$name"; \
		else \
			echo "  [FAIL] $$name (failed with unexpected diagnostic):"; cat $$TMP_LOG; exit 1; \
		fi; \
	}; \
	run_case "FAIL_UNDECLARED_ACTION" "undeclared" "Undeclared Action Detection" && \
	run_case "FAIL_DUPLICATE_ACTION_ID" "override" "Duplicate Action ID Collision"

example: $(LIB)
	$(CC) $(CFLAGS) -Iexamples examples/minimal.c $(LIB) -o $(EXAMPLE_BIN)
	@printf "\n--- Running Minimal Example ---\n"
	@./$(EXAMPLE_BIN)

clean:
	rm -rf build

-include $(DEPS)
