# Makefile

# Build targets:
# make sim # build the functional simulator
# make all # build the functional simulator and all tests
# make tests # build all assembly tests
# make check # build everything, run every test, and compare against reference outputs
# make check TEST=fib # run and check only test/fib.s (space-separate multiple names in quotes)
# make clean # removes sim, and all .bin, .elf, .out, and .log files in test/

# Note: If you're having trouble getting the assembler and objcopy executables to work,
# you might need to mark those files as executables using 'chmod +x filename'

# This Makefile checks if it's being run on Adroit (adroit.princeton.edu).
# Building elsewhere is allowed but unsupported: src/UtilityFunctions.o and the tools in bin/ are
# x86-64 Linux binaries, so a local build will generally only work on x86-64 Linux (e.g., WSL on
# Windows). The project is only tested to work as expected on Adroit.
ALLOWED_HOSTS = adroit%
CURRENT_HOST := $(shell hostname -s 2>/dev/null || hostname)

HOST_OK := $(filter $(ALLOWED_HOSTS),$(CURRENT_HOST))
ifeq ($(HOST_OK),)
    $(warning You are not running on Adroit (adroit.princeton.edu). Attempting a build anyway.)
    $(warning This is unsupported and discouraged; our code is only tested to work as expected on Adroit.)
endif

# Compiler settings
CC = g++
# Note: All builds will contain debug information
CFLAGS = --std=c++14 -Wall -g -pedantic -O2

# Source and header files
SIM_SRC = sim.cpp
SIM_SRCS = $(addprefix src/, $(SIM_SRC))
COMMON_HDRS = $(wildcard src/*.h)
COMMON_OBJS = $(wildcard src/*.o)

ASSEMBLY_TESTS = $(wildcard test/*.s)
ASSEMBLY_TARGETS = $(ASSEMBLY_TESTS:.s=.bin)

# Override these to use your own RISC-V binutils, e.g. make ASSEMBLER=riscv64-unknown-elf-as
ASSEMBLER ?= bin/riscv64-elf-as
OBJCOPY ?= bin/riscv64-elf-objcopy

# Tests run by 'make check' (default: all of them) and the per-test time limit
TEST ?= $(ASSEMBLY_TESTS:test/%.s=%)
TIMEOUT = 10s
# Use GNU timeout (named gtimeout on some systems); if neither exists, tests run without a time limit
TIMEOUT_CMD := $(shell command -v timeout 2>/dev/null || command -v gtimeout 2>/dev/null)
ifneq ($(TIMEOUT_CMD),)
    TIMEOUT_CMD += $(TIMEOUT)
endif

# Main targets
all: sim tests

sim: $(SIM_SRCS) $(COMMON_HDRS)
	$(CC) $(CFLAGS) -o sim $(COMMON_OBJS) $(SIM_SRCS)

# Test targets
tests: $(ASSEMBLY_TARGETS)

$(ASSEMBLY_TARGETS) : test/%.bin : test/%.s
	$(ASSEMBLER) test/$*.s -o test/$*.elf
	$(OBJCOPY) test/$*.elf -j .text -O binary test/$*.bin

# Runs each test and saves its outputs as test/NAME.mem_state.out and test/NAME.reg_state.out
# (simulator messages go to test/NAME.log). If test/NAME.mem_state.ref and test/NAME.reg_state.ref
# exist, the outputs must match them exactly to pass; otherwise the test is reported as NO REF.
check: sim tests
	@pass=0; fail=0; noref=0; \
	for t in $(TEST); do \
		rm -f mem_state.out reg_state.out; \
		$(TIMEOUT_CMD) ./sim test/$$t.bin > test/$$t.log 2>&1; rc=$$?; \
		if [ $$rc -eq 124 ]; then \
			echo "FAIL    $$t (timed out after $(TIMEOUT), infinite loop?)"; fail=$$((fail+1)); continue; \
		fi; \
		if [ ! -f mem_state.out ] || [ ! -f reg_state.out ]; then \
			echo "FAIL    $$t (sim exited with code $$rc without writing outputs, see test/$$t.log)"; fail=$$((fail+1)); continue; \
		fi; \
		mv mem_state.out test/$$t.mem_state.out; mv reg_state.out test/$$t.reg_state.out; \
		if [ ! -f test/$$t.mem_state.ref ] || [ ! -f test/$$t.reg_state.ref ]; then \
			echo "NO REF  $$t (outputs saved to test/$$t.mem_state.out and test/$$t.reg_state.out)"; noref=$$((noref+1)); continue; \
		fi; \
		if cmp -s test/$$t.mem_state.out test/$$t.mem_state.ref && cmp -s test/$$t.reg_state.out test/$$t.reg_state.ref; then \
			echo "PASS    $$t"; pass=$$((pass+1)); \
		else \
			echo "FAIL    $$t (sim exit code $$rc, messages in test/$$t.log)"; fail=$$((fail+1)); \
			for kind in reg_state mem_state; do \
				cmp -s test/$$t.$$kind.out test/$$t.$$kind.ref && continue; \
				echo "        $$kind differs (< expected, > yours):"; \
				diff test/$$t.$$kind.ref test/$$t.$$kind.out | head -n 20 | sed 's/^/        /'; \
			done; \
		fi; \
	done; \
	echo "$$pass passed, $$fail failed, $$noref without reference output"; \
	[ $$fail -eq 0 ]

# Clean function
clean:
	rm -f sim
	rm -f test/*.bin test/*.elf test/*.out test/*.log

# Phony targets
.PHONY: all tests check clean

# To dump elf:
# bin/riscv64-elf-objdump -D -j .text -M no-aliases test/*.elf
