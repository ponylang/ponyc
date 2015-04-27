# Determine the operating system
OSTYPE ?=

ifeq ($(OS),Windows_NT)
  OSTYPE = windows
else
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Linux)
    OSTYPE = linux
  endif

  ifeq ($(UNAME_S),Darwin)
    OSTYPE = osx
  endif
endif

# Default settings (silent debug build).
config ?= debug
arch ?= native
tag ?= $(shell git describe --tags --always)

LIB_EXT ?= a
BUILD_FLAGS = -mcx16 -march=$(arch) -Werror -Wconversion \
  -Wno-sign-conversion -Wextra -Wall
LINKER_FLAGS =
ALL_CFLAGS = -std=gnu11 -DPONY_VERSION=\"$(tag)\"
ALL_CXXFLAGS = -std=gnu++11

PONY_BUILD_DIR   ?= build/$(config)
PONY_SOURCE_DIR  ?= src
PONY_TEST_DIR ?= test

ifdef use
  ifneq (,$(filter $(use), numa))
    ALL_CFLAGS += -DUSE_NUMA
    LINK_NUMA = true
    PONY_BUILD_DIR := $(PONY_BUILD_DIR)-numa
  endif

  ifneq (,$(filter $(use), valgrind))
    ALL_CFLAGS += -DUSE_VALGRIND
    PONY_BUILD_DIR := $(PONY_BUILD_DIR)-valgrind
  endif
endif

ifndef verbose
  SILENT = @
else
  SILENT =
endif

ifdef config
  ifeq (,$(filter $(config),debug release))
    $(error Unknown configuration "$(config)")
  endif
endif

ifeq ($(config),release)
  BUILD_FLAGS += -O3 -DNDEBUG
else
  BUILD_FLAGS += -g -DDEBUG
endif

ifneq (,$(shell which llvm-config 2> /dev/null))
  LLVM_CONFIG = llvm-config
endif

ifneq (,$(shell which llvm-config-3.6 2> /dev/null))
  LLVM_CONFIG = llvm-config-3.6
endif

ifndef LLVM_CONFIG
  $(error No LLVM 3.6 installation found!)
endif

$(shell mkdir -p $(PONY_BUILD_DIR))

lib   := $(PONY_BUILD_DIR)
bin   := $(PONY_BUILD_DIR)
tests := $(PONY_BUILD_DIR)
obj  := $(PONY_BUILD_DIR)/obj

# Libraries. Defined as
# (1) a name and output directory
libponyc  := $(lib)
libponycc := $(lib)
libponyrt := $(lib)

ifeq ($(OSTYPE),linux)
  libponyrt-pic := $(lib)
endif

# Define special case rules for a targets source files. By default
# this makefile assumes that a targets source files can be found
# relative to a parent directory of the same name in $(PONY_SOURCE_DIR).
# Note that it is possible to collect files and exceptions with
# arbitrarily complex shell commands, as long as ':=' is used
# for definition, instead of '='.
ifneq ($(OSTYPE),windows)
  libponyc.except += src/libponyc/platform/signed.cc
  libponyc.except += src/libponyc/platform/unsigned.cc
  libponyc.except += src/libponyc/platform/vcvars.c
endif

# Handle platform specific code to avoid "no symbols" warnings.
libponyrt.except =

ifneq ($(OSTYPE),windows)
  libponyrt.except += src/libponyrt/asio/iocp.c
  libponyrt.except += src/libponyrt/lang/win_except.c
endif

ifneq ($(OSTYPE),linux)
  libponyrt.except += src/libponyrt/asio/epoll.c
endif

ifneq ($(OSTYPE),osx)
  libponyrt.except += src/libponyrt/asio/kqueue.c
endif

libponyrt.except += src/libponyrt/asio/sock.c
libponyrt.except += src/libponyrt/dist/dist.c
libponyrt.except += src/libponyrt/dist/proto.c

ifeq ($(OSTYPE),linux)
  libponyrt-pic.dir := src/libponyrt
  libponyrt-pic.except := $(libponyrt.except)
endif

# Third party, but requires compilation. Defined as
# (1) a name and output directory.
# (2) a list of the source files to be compiled.
libgtest := $(lib)
libgtest.dir := lib/gtest
libgtest.files := $(libgtest.dir)/gtest_main.cc $(libgtest.dir)/gtest-all.cc

ifeq ($(OSTYPE), linux)
  libraries := libponyc libponyrt libponyrt-pic libgtest
else
  libraries := libponyc libponyrt libgtest
endif

# Third party, but prebuilt. Prebuilt libraries are defined as
# (1) a name (stored in prebuilt)
# (2) the linker flags necessary to link against the prebuilt library/libraries.
# (3) a list of include directories for a set of libraries.
# (4) a list of the libraries to link against.
llvm.ldflags := $(shell $(LLVM_CONFIG) --ldflags)
llvm.include := -isystem $(shell $(LLVM_CONFIG) --includedir)
llvm.libs    := $(shell $(LLVM_CONFIG) --libs) -lz -lncurses

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
libponyc.include := -I src/common/ $(llvm.include)/
libponycc.include := -I src/common/ $(llvm.include)/
libponyrt.include := -I src/common/ -I src/libponyrt/
libponyrt-pic.include := $(libponyrt.include)

libponyc.tests.include := -I src/common/ -I src/libponyc/ -isystem lib/gtest/
libponyrt.tests.include := -I src/common/ -I src/libponyrt/ -isystem lib/gtest/

ponyc.include := -I src/common/
libgtest.include := -isystem lib/gtest/

# target specific build options
libponyc.buildoptions = -D__STDC_CONSTANT_MACROS
libponyc.buildoptions += -D__STDC_FORMAT_MACROS
libponyc.buildoptions += -D__STDC_LIMIT_MACROS

ifeq ($(OSTYPE), linux)
  libponyrt-pic.buildoptions += -fpic
endif

# target specific disabling of build options
libgtest.disable = -Wconversion -Wno-sign-conversion -Wextra

# Link relationships.
ponyc.links = libponyc libponyrt llvm
libponyc.tests.links = libgtest libponyc libponyrt llvm
libponyrt.tests.links = libgtest libponyrt

ifeq ($(OSTYPE),linux)
  ifeq ($(LINK_NUMA),true)
    ponyc.links += numa
		libponyc.tests.links += numa
		libponyrt.tests.links += numa
  endif

  ponyc.links += pthread dl
  libponyc.tests.links += pthread dl
  libponyrt.tests.links += pthread
endif

# Overwrite the default linker for a target.
ponyc.linker = $(CXX) #compile as C but link as CPP (llvm)

# make targets
targets := $(libraries) $(binaries) $(tests)

.PHONY: all $(targets)
all: $(targets)
	@:

# Dependencies
libponyc.depends := libponyrt
libponyc.tests.depends := libponyc libgtest
libponyrt.tests.depends := libponyrt libgtest
ponyc.depends := libponyc libponyrt

# Generic make section, edit with care.
##########################################################################
#                                                                        #
# DIRECTORY: Determines the source dir of a specific target              #
#                                                                        #
# ENUMERATE: Enumerates input and output files for a specific target     #
#                                                                        #
# CONFIGURE_COMPILER: Chooses a C or C++ compiler depending on the       #
#                     target file.                                       #
#                                                                        #
# CONFIGURE_LIBS: Builds a string of libraries to link for a targets     #
#                 link dependency.                                       #
#                                                                        #
# CONFIGURE_LINKER: Assembles the linker flags required for a target.    #
#                                                                        #
# EXPAND_COMMAND: Macro that expands to a proper make command for each   #
#                 target.                                                #
#                                                                        #
##########################################################################
define DIRECTORY
  $(eval sourcedir := )
  $(eval outdir := $(obj)/$(1))

  ifdef $(1).dir
    sourcedir := $($(1).dir)
  else ifneq ($$(filter $(1),$(tests)),)
    sourcedir := $(PONY_TEST_DIR)/$(subst .tests,,$(1))
    outdir := $(obj)/tests/$(subst .tests,,$(1))
  else
    sourcedir := $(PONY_SOURCE_DIR)/$(1)
  endif
endef

define ENUMERATE
  $(eval sourcefiles := )

  ifdef $(1).files
    sourcefiles := $$($(1).files)
  else
    sourcefiles := $$(shell find $$(sourcedir) -type f -name "*.c" -or -name\
      "*.cc" | grep -v '.*/\.')
  endif

  ifdef $(1).except
    sourcefiles := $$(filter-out $($(1).except),$$(sourcefiles))
  endif
endef

define CONFIGURE_COMPILER
  $(eval compiler := $(CC))
  $(eval flags := $(ALL_CFLAGS) $(CFLAGS) $(CXXFLAGS))

  ifeq ($(suffix $(1)),.cc)
    compiler := $(CXX)
    flags := $(ALL_CXXFLAGS)
  endif
endef

define CONFIGURE_LIBS
  ifneq (,$$(filter $(1),$(prebuilt)))
    linkcmd += $($(1).ldflags)
    libs += $($(1).libs)
  else
    libs += -l$(subst lib,,$(1))
  endif
endef

define CONFIGURE_LINKER
  $(eval linkcmd := $(LINKER_FLAGS) -L $(lib))
  $(eval linker := $(CC))
  $(eval libs :=)

  ifdef $(1).linker
    linker := $($(1).linker)
  else ifneq (,$$(filter .cc,$(suffix $(sourcefiles))))
    linker := $(CXX)
  endif

  $(foreach lk,$($(1).links),$(eval $(call CONFIGURE_LIBS,$(lk))))
  linkcmd += $(libs) $($(1).linkoptions)
endef

define PREPARE
  $(eval $(call DIRECTORY,$(1)))
  $(eval $(call ENUMERATE,$(1)))
  $(eval $(call CONFIGURE_LINKER,$(1)))
  $(eval objectfiles  := $(subst $(sourcedir)/,$(outdir)/,$(addsuffix .o,\
    $(sourcefiles))))
  $(eval dependencies := $(subst .c,,$(subst .cc,,$(subst .o,.d,\
    $(objectfiles)))))
endef

define EXPAND_OBJCMD
$(eval file := $(subst .o,,$(1)))
$(eval $(call CONFIGURE_COMPILER,$(file)))

$(subst .c,,$(subst .cc,,$(1))): $(subst $(outdir)/,$(sourcedir)/,$(file))
	@echo '$$(notdir $$<)'
	@mkdir -p $$(dir $$@)
	$(SILENT)$(compiler) -MMD -MP $(filter-out $($(2).disable),$(BUILD_FLAGS)) \
    $(flags) $($(2).buildoptions) -c -o $$@ $$<  $($(2).include)
endef

define EXPAND_COMMAND
$(eval $(call PREPARE,$(1)))
$(eval ofiles := $(subst .c,,$(subst .cc,,$(objectfiles))))
$(eval depends := )
$(foreach d,$($(1).depends),$(eval depends += $($(d))/$(d).$(LIB_EXT)))

ifneq ($(filter $(1),$(libraries)),)
$($(1))/$(1).$(LIB_EXT): $(depends) $(ofiles)
	@echo 'Linking $(1)'
	$(SILENT)$(AR) -rcs $$@ $(ofiles)
$(1): $($(1))/$(1).$(LIB_EXT)
else
$($(1))/$(1): $(depends) $(ofiles)
	@echo 'Linking $(1)'
	$(SILENT)$(linker) -o $$@ $(ofiles) $(linkcmd)
$(1): $($(1))/$(1)
endif

$(foreach ofile,$(objectfiles),$(eval $(call EXPAND_OBJCMD,$(ofile),$(1))))
-include $(dependencies)
endef

$(foreach target,$(targets),$(eval $(call EXPAND_COMMAND,$(target))))

define EXPAND_RELEASE
$(eval branch := $(shell git symbolic-ref HEAD | sed -e 's,.*/\(.*\),\1,'))
$(eval version_string := $(subst ., ,$(version)))
$(eval major := $(firstword $(version_string)))
$(eval minor := $(word 2, $(version_string)))
$(eval patch := $(word 3, $(version_string)))
ifneq ("$(branch)","release")
prerelease:
	$$(error "Releases not allowed on $(branch) branch.")
else
ifndef version
prerelease:
	$$(error "No version number specified.")
else
$(eval ALL_CFLAGS += -DPONY_VERSION_MAJOR=$(major) -DPONY_VERSION_MINOR=$(minor) -DPONY_VERSION_PATCH=$(patch))
$(eval ALL_CFLAGS = $(subst -DPONY_VERSION=\"$(tag)\", -DPONY_VERSION=\"$(version)\", $(ALL_CFLAGS)))
prerelease: libponyc libponyrt ponyc
	@while [ -z "$$$$CONTINUE" ]; do \
	read -r -p "New version number: $(version). Are you sure? [y/N]: " CONTINUE; \
	done ; \
	[ $$$$CONTINUE = "y" ] || [ $$$$CONTINUE = "Y" ] || (echo "Release aborted."; exit 1;)
	@echo "Releasing ponyc v$(version)."
endif
endif
endef

$(eval $(call EXPAND_RELEASE))

release: prerelease
	@git tag $(version)
	@git push origin $(version)

deb:
ifndef name
	$(error No installer name specified.)
ifndef description
	$(error No package description specified.)
else 
	TARGET=$(name)-$(tag) \
	PACKAGE=$(PONY_BUILD_DIR)/$TARGET
	mkdir $(PACKAGE)
	mkdir $PACKAGE/DEBIAN
	touch $PACKAGE/DEBIAN/control
	cat <<EOT >> $PACKAGE/DEBIAN/control
	Package: $(name)
	Version: $(tag)
	Section: base
	Priority: optional
	Architecture: amd64
	Maintainer: Pony Buildbot <buildbot@lists.ponylang.org>
	Description: $(description)
	EOT
	mkdir -p $PACKAGE/usr/bin
	mkdir -p $PACKAGE/usr/include
	mkdir -p $PACKAGE/usr/lib
	mkdir -p $PACKAGE/usr/lib/pony/$(tag)/bin
	mkdir -p $PACKAGE/usr/lib/pony/$(tag)/include
	mkdir -p $PACKAGE/usr/lib/pony/$(tag)/lib
	cp $(PONY_BUILD_DIR)/release/libponyc.a $PACKAGE/usr/lib/pony/$(tag)/lib
	cp $(PONY_BUILD_DIR)/release/libponyrt.a $PACKAGE/usr/lib/pony/$(tag)/lib
	cp $(PONY_BUILD_DIR)/release/ponyc $PACKAGE/usr/lib/pony/$(tag)/bin
	cp src/libponyrt/pony.h $PACKAGE/usr/lib/pony/$(tag)/include
	cp -r packages $PACKAGE/usr/lib/pony/$(tag)/
	dpkg-deb --build $PACKAGE $(PONY_BUILD_DIR)
endif
endif

test: all
	@$(PONY_BUILD_DIR)/libponyc.tests
	@$(PONY_BUILD_DIR)/libponyrt.tests

stats:
	@echo
	@echo '------------------------------'
	@echo 'Compiler and standard library '
	@echo '------------------------------'
	@echo
	@cloc --read-lang-def=pony.cloc src packages
	@echo
	@echo '------------------------------'
	@echo 'Test suite:'
	@echo '------------------------------'
	@echo
	@cloc --read-lang-def=pony.cloc test

clean:
	@rm -rf $(PONY_BUILD_DIR)
	-@rmdir build 2>/dev/null ||:
	@echo 'Repository cleaned ($(PONY_BUILD_DIR)).'

help:
	@echo
	@echo 'Usage: make [config=name] [arch=name] [use=opt,...] [target]'
	@echo
	@echo 'CONFIGURATIONS:'
	@echo '  debug (default)'
	@echo '  release'
	@echo
	@echo 'ARCHITECTURE:'
	@echo '  native (default)'
	@echo '  [any compiler supported architecture]'
	@echo
	@echo 'USE OPTIONS:'
	@echo '   numa'
	@echo '   valgrind'
	@echo
	@echo 'TARGETS:'
	@echo '  libponyc          Pony compiler library'
	@echo '  libponyrt         Pony runtime'
	@echo '  libponyrt-pic     Pony runtime -fpic'
	@echo '  libponyc.tests    Test suite for libponyc'
	@echo '  libponyrt.tests   Test suite for libponyrt'
	@echo '  ponyc             Pony compiler executable'
	@echo
	@echo '  all               Build all of the above (default)'
	@echo '  stats             Print Pony cloc statistics'
	@echo '  clean             Delete all build files'
	@echo
