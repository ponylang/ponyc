# Default settings (silent debug build).
LIB_EXT ?= a
BUILD_FLAGS = -mcx16 -march=native -Werror -Wall
LINKER_FLAGS =
ALL_CFLAGS = -std=gnu11
ALL_CXXFLAGS = -std=gnu++11

ifeq ($(config),release)
  BUILD_FLAGS += -O3 -DNDEBUG
  LINKER_FLAGS += -fuse-ld=gold
  DEFINES +=
else
  config = debug
  BUILD_FLAGS += -g -DDEBUG
endif

PONY_BUILD_DIR   ?= build/$(config)
PONY_SOURCE_DIR  ?= src
PONY_TEST_DIR ?= test

lib   := $(PONY_BUILD_DIR)/lib
bin   := $(PONY_BUILD_DIR)/bin
tests := $(PONY_BUILD_DIR)/tests

$(shell mkdir -p $(lib))
$(shell mkdir -p $(bin))
$(shell mkdir -p $(tests))

obj  := $(PONY_BUILD_DIR)/obj

# Libraries. Defined as
# (1) a name and output directory
libponyc  := $(lib)
libponycc := $(lib)
libponyrt := $(lib)

# Define special case rules for a targets source files. By default
# this makefile assumes that a targets source files can be found
# relative to a parent directory of the same name in $(PONY_SOURCE_DIR).
# Note that it is possible to collect files and exceptions with
# arbitrarily complex shell commands, as long as ':=' is used
# for definition, instead of '='.
libponycc.dir := src/libponyc
libponycc.files := src/libponyc/debug/dwarf.cc
libponycc.files += src/libponyc/debug/symbols.cc
libponycc.files += src/libponyc/codegen/host.cc

libponyc.except := $(libponycc.files)
libponyc.except += src/libponyc/platform/signed.cc
libponyc.except += src/libponyc/platform/unsigned.cc
libponyc.except += src/libponyc/platform/vcvars.c

# Third party, but requires compilation. Defined as
# (1) a name and output directory.
# (2) a list of the source files to be compiled.
gtest := $(lib)
gtest.dir := lib/gtest
gtest.files := $(gtest.dir)/gtest_main.cc $(gtest.dir)/gtest-all.cc

libraries := libponyc libponycc libponyrt gtest

# Third party, but prebuilt. Prebuilt libraries are defined as
# (1) a name (stored in prebuilt)
# (2) the linker flags necessary to link against the prebuilt library/libraries.
# (3) a list of include directories for a set of libraries.
# (4) a list of the libraries to link against.
llvm.ldflags := $(shell llvm-config-3.5 --ldflags)
llvm.include := $(shell llvm-config-3.5 --includedir)
llvm.libs    := $(shell llvm-config-3.5 --libs) -lz -lcurses

prebuilt := llvm

# Binaries. Defined as
# (1) a name and output directory.
ponyc := $(bin)

binaries := ponyc

# Tests suites are directly attached to the libraries they test.
libponyc.tests  := $(tests)
libponyrt.tests := $(tests)

tests := libponyc.tests libponyrt.tests

# Define include paths for targets if necessary. Note that these include paths
# will automatically apply to the test suite of a target as well.
libponyc.include := src/common/ $(llvm.include)/
libponycc.include := src/common/ $(llvm.include)/
libponyrt.include := src/common/ src/libponyrt/

ponyc.include := src/common/
gtest.include := lib/gtest/

# target specific build options
libponyc.options = -D__STDC_CONSTANT_MACROS
libponyc.options += -D__STDC_FORMAT_MACROS
libponyc.options += -D__STDC_LIMIT_MACROS
libponyc.options += -Wconversion -Wno-sign-conversion

libponyrt.options = -Wconversion -Wno-sign-conversion

libponycc.options = -D__STDC_CONSTANT_MACROS
libponycc.options += -D__STDC_FORMAT_MACROS
libponycc.options += -D__STDC_LIMIT_MACROS

ponyc.options = -Wconversion -Wno-sign-conversion

# Link relationships.
ponyc.links = libponyc libponycc libponyrt llvm
libponyc.tests.links = libponyc libponycc libponyrt llvm gtest
libponyrt.tests.links = libponyrt gtest

# make targets
targets := $(libraries) $(binaries) $(tests)

.PHONY: all $(targets)
all: $(targets)

# Dependencies
libponycc:
libponyrt:
libponyc: libponycc libponyrt
libponyc.tests: libponyc gtest
libponyrt.tests: libponyrt gtest
ponyc: libponyc
gtest:

# Generic make section, edit with care.
-include Rules.mk

$(foreach target,$(targets),$(eval $(call EXPAND_COMMAND,$(target))))

stats:
	@echo
	@echo '------------------------------'
	@echo 'Compiler and standard library '
	@echo '------------------------------'
	@echo
	@cloc --read-lang-def=misc/pony.cloc src packages
	@echo
	@echo '------------------------------'
	@echo 'Test suite:'
	@echo '------------------------------'
	@echo
	@cloc --read-lang-def=misc/pony.cloc test

clean:
	@rm -rf build
	@echo 'Repository cleaned.'

help:
	@echo
	@echo 'Usage: make [config=name] [target]'
	@echo
	@echo "CONFIGURATIONS:"
	@echo "  debug"
	@echo "  release"
	@echo
	@echo 'TARGETS:'
	@echo '  libponyc          Pony compiler library'
	@echo '  libponycc         Pony compiler host info and debugger support'
	@echo '  libponyrt         Pony runtime'
	@echo '  libponyc.tests    Test suite for libponyc'
	@echo '  libponyrt.tests   Test suite for libponyrt'
	@echo '  ponyc             Pony compiler executable'
	@echo
	@echo '  all               Build all of the above (default)'
	@echo '  stats             Print Pony cloc statistics'
	@echo '  clean             Delete all build files'
	@echo
