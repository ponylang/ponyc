# Determine the operating system
OSTYPE ?=

ifeq ($(OS),Windows_NT)
  OSTYPE = windows
else
  UNAME_S := $(shell uname -s)

  ifeq ($(UNAME_S),Linux)
    OSTYPE = linux

    ifneq (,$(shell which gcc-ar 2> /dev/null))
      AR = gcc-ar
    endif
  endif

  ifeq ($(UNAME_S),Darwin)
    OSTYPE = osx
    lto := yes
    AR := /usr/bin/ar
  endif

  ifeq ($(UNAME_S),FreeBSD)
    OSTYPE = freebsd
    CXX = c++
  endif
endif

ifdef LTO_PLUGIN
  lto := yes
endif

# Default settings (silent debug build).
config ?= debug
arch ?= native
bits ?= $(shell getconf LONG_BIT)

ifndef verbose
  SILENT = @
else
  SILENT =
endif

ifneq ($(wildcard .git),)
tag := $(shell git describe --tags --always)
git := yes
else
tag := $(shell cat VERSION)
git := no
endif

package_version := $(tag)
archive = ponyc-$(package_version).tar
package = build/ponyc-$(package_version)

symlink := yes

ifdef destdir
  ifndef prefix
    symlink := no
  endif
endif

ifeq ($(OSTYPE),osx)
  symlink.flags = -sf
else
  symlink.flags = -srf
endif

prefix ?= /usr/local
destdir ?= $(prefix)/lib/pony/$(tag)

LIB_EXT ?= a
BUILD_FLAGS = -march=$(arch) -Werror -Wconversion \
  -Wno-sign-conversion -Wextra -Wall
LINKER_FLAGS = -march=$(arch)
AR_FLAGS = -rcs
ALL_CFLAGS = -std=gnu11 -fexceptions \
  -DPONY_VERSION=\"$(tag)\" -DPONY_COMPILER=\"$(CC)\" -DPONY_ARCH=\"$(arch)\" \
  -DPONY_BUILD_CONFIG=\"$(config)\"
ALL_CXXFLAGS = -std=gnu++11 -fno-rtti

# Determine pointer size in bits.
BITS := $(bits)

ifeq ($(BITS),64)
  BUILD_FLAGS += -mcx16
  LINKER_FLAGS += -mcx16
endif

PONY_BUILD_DIR   ?= build/$(config)
PONY_SOURCE_DIR  ?= src
PONY_TEST_DIR ?= test

ifdef use
  ifneq (,$(filter $(use), valgrind))
    ALL_CFLAGS += -DUSE_VALGRIND
    PONY_BUILD_DIR := $(PONY_BUILD_DIR)-valgrind
  endif

  ifneq (,$(filter $(use), pooltrack))
    ALL_CFLAGS += -DUSE_POOLTRACK
    PONY_BUILD_DIR := $(PONY_BUILD_DIR)-pooltrack
  endif

  ifneq (,$(filter $(use), telemetry))
    ALL_CFLAGS += -DUSE_TELEMETRY
    PONY_BUILD_DIR := $(PONY_BUILD_DIR)-telemetry
  endif
endif

ifdef config
  ifeq (,$(filter $(config),debug release))
    $(error Unknown configuration "$(config)")
  endif
endif

ifeq ($(config),release)
  BUILD_FLAGS += -O3 -DNDEBUG

  ifeq ($(lto),yes)
    BUILD_FLAGS += -flto -DPONY_USE_LTO
    LINKER_FLAGS += -flto

    ifdef LTO_PLUGIN
      AR_FLAGS += --plugin $(LTO_PLUGIN)
    endif

    ifneq (,$(filter $(OSTYPE),linux freebsd))
      LINKER_FLAGS += -fuse-linker-plugin -fuse-ld=gold
    endif
  endif
else
  BUILD_FLAGS += -g -DDEBUG
endif

ifeq ($(OSTYPE),osx)
  ALL_CFLAGS += -mmacosx-version-min=10.8
  ALL_CXXFLAGS += -stdlib=libc++ -mmacosx-version-min=10.8
endif

ifndef LLVM_CONFIG
  ifneq (,$(shell which llvm-config-3.8 2> /dev/null))
    LLVM_CONFIG = llvm-config-3.8
  else ifneq (,$(shell which llvm-config-3.7 2> /dev/null))
    LLVM_CONFIG = llvm-config-3.7
  else ifneq (,$(shell which llvm-config-3.6 2> /dev/null))
    LLVM_CONFIG = llvm-config-3.6
  else ifneq (,$(shell which llvm-config38 2> /dev/null))
    LLVM_CONFIG = llvm-config38
  else ifneq (,$(shell which llvm-config37 2> /dev/null))
    LLVM_CONFIG = llvm-config37
  else ifneq (,$(shell which llvm-config36 2> /dev/null))
    LLVM_CONFIG = llvm-config36
  else ifneq (,$(shell which /usr/local/opt/llvm/bin/llvm-config 2> /dev/null))
    LLVM_CONFIG = /usr/local/opt/llvm/bin/llvm-config
  else ifneq (,$(shell which llvm-config 2> /dev/null))
    LLVM_CONFIG = llvm-config
  endif
endif

ifndef LLVM_CONFIG
  $(error No LLVM installation found!)
endif

$(shell mkdir -p $(PONY_BUILD_DIR))

lib   := $(PONY_BUILD_DIR)
bin   := $(PONY_BUILD_DIR)
tests := $(PONY_BUILD_DIR)
obj   := $(PONY_BUILD_DIR)/obj

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
  ifneq ($(OSTYPE),freebsd)
    libponyrt.except += src/libponyrt/asio/kqueue.c
  endif
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
# (2) the linker flags necessary to link against the prebuilt libraries
# (3) a list of include directories for a set of libraries
# (4) a list of the libraries to link against
llvm.ldflags := $(shell $(LLVM_CONFIG) --ldflags)
llvm.include.dir := $(shell $(LLVM_CONFIG) --includedir)
include.paths := $(shell echo | cc -v -E - 2>&1)
ifeq (,$(findstring $(llvm.include.dir),$(include.paths)))
# LLVM include directory is not in the existing paths;
# put it at the top of the system list
llvm.include := -isystem $(llvm.include.dir)
else
# LLVM include directory is already on the existing paths;
# do nothing
llvm.include :=
endif
llvm.libs    := $(shell $(LLVM_CONFIG) --libs) -lz -lncurses

ifeq ($(OSTYPE), freebsd)
  llvm.libs += -lpthread
endif

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
libponyc.include := -I src/common/ -I src/libponyrt/ $(llvm.include)
libponycc.include := -I src/common/ $(llvm.include)
libponyrt.include := -I src/common/ -I src/libponyrt/
libponyrt-pic.include := $(libponyrt.include)

libponyc.tests.include := -I src/common/ -I src/libponyc/ $(llvm.include) \
  -isystem lib/gtest/
libponyrt.tests.include := -I src/common/ -I src/libponyrt/ -isystem lib/gtest/

ponyc.include := -I src/common/ -I src/libponyrt/ $(llvm.include)
libgtest.include := -isystem lib/gtest/

ifneq (,$(filter $(OSTYPE), osx freebsd))
  libponyrt.include += -I /usr/local/include
endif

# target specific build options
libponyc.buildoptions = -D__STDC_CONSTANT_MACROS
libponyc.buildoptions += -D__STDC_FORMAT_MACROS
libponyc.buildoptions += -D__STDC_LIMIT_MACROS

libponyc.tests.buildoptions = -D__STDC_CONSTANT_MACROS
libponyc.tests.buildoptions += -D__STDC_FORMAT_MACROS
libponyc.tests.buildoptions += -D__STDC_LIMIT_MACROS

ponyc.buildoptions = $(libponyc.buildoptions)

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
  ponyc.links += pthread dl
  libponyc.tests.links += pthread dl
  libponyrt.tests.links += pthread dl
endif

# Overwrite the default linker for a target.
ponyc.linker = $(CXX) #compile as C but link as CPP (llvm)

# make targets
targets := $(libraries) $(binaries) $(tests)

.PHONY: all $(targets) install uninstall clean stats deploy prerelease
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
  ifeq ($(suffix $(1)),.cc)
    compiler := $(CXX)
    flags := $(ALL_CXXFLAGS) $(CXXFLAGS)
  endif

  ifeq ($(suffix $(1)),.c)
    compiler := $(CC)
    flags := $(ALL_CFLAGS) $(CFLAGS)
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
	$(SILENT)$(AR) $(AR_FLAGS) $$@ $(ofiles)
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
ifneq ($(branch),master)
prerelease:
	$$(error "Releases not allowed on $(branch) branch.")
else
ifndef version
prerelease:
	$$(error "No version number specified.")
else
$(eval tag := $(version))
$(eval unstaged := $(shell git status --porcelain 2>/dev/null | wc -l))
ifneq ($(unstaged),0)
prerelease:
	$$(error "Detected unstaged changes. Release aborted")
else
prerelease: libponyc libponyrt ponyc
	@while [ -z "$$$$CONTINUE" ]; do \
	read -r -p "New version number: $(tag). Are you sure? [y/N]: " CONTINUE; \
	done ; \
	[ $$$$CONTINUE = "y" ] || [ $$$$CONTINUE = "Y" ] || (echo "Release aborted."; exit 1;)
	@echo "Releasing ponyc v$(tag)."
endif
endif
endif
endef

define EXPAND_INSTALL
install: libponyc libponyrt ponyc
	@mkdir -p $(destdir)/bin
	@mkdir -p $(destdir)/lib
	@mkdir -p $(destdir)/include
	$(SILENT)cp $(PONY_BUILD_DIR)/libponyrt.a $(destdir)/lib
	$(SILENT)cp $(PONY_BUILD_DIR)/libponyc.a $(destdir)/lib
	$(SILENT)cp $(PONY_BUILD_DIR)/ponyc $(destdir)/bin
	$(SILENT)cp src/libponyrt/pony.h $(destdir)/include
	$(SILENT)cp -r packages $(destdir)/
ifeq ($$(symlink),yes)
	@mkdir -p $(prefix)/bin
	@mkdir -p $(prefix)/lib
	@mkdir -p $(prefix)/include
	$(SILENT)ln $(symlink.flags) $(destdir)/bin/ponyc $(prefix)/bin/ponyc
	$(SILENT)ln $(symlink.flags) $(destdir)/lib/libponyrt.a $(prefix)/lib/libponyrt.a
	$(SILENT)ln $(symlink.flags) $(destdir)/lib/libponyc.a $(prefix)/lib/libponyc.a
	$(SILENT)ln $(symlink.flags) $(destdir)/include/pony.h $(prefix)/include/pony.h
endif
endef

$(eval $(call EXPAND_INSTALL))

uninstall:
	-$(SILENT)rm -rf $(destdir) 2>/dev/null ||:
	-$(SILENT)rm $(prefix)/bin/ponyc 2>/dev/null ||:
	-$(SILENT)rm $(prefix)/lib/libponyrt.a 2>/dev/null ||:
	-$(SILENT)rm $(prefix)/lib/libponyc.a 2>/dev/null ||:
	-$(SILENT)rm $(prefix)/include/pony.h 2>/dev/null ||:

test: all
	@$(PONY_BUILD_DIR)/libponyc.tests
	@$(PONY_BUILD_DIR)/libponyrt.tests
	@$(PONY_BUILD_DIR)/ponyc -d -s packages/stdlib
	@./stdlib
	@rm stdlib

ifeq ($(git),yes)
setversion:
	@echo $(tag) > VERSION

$(eval $(call EXPAND_RELEASE))

release: prerelease setversion
	$(SILENT)git add VERSION
	$(SILENT)git commit -m "Releasing version $(tag)"
	$(SILENT)git tag $(tag)
	$(SILENT)git push
	$(SILENT)git push --tags
	$(SILENT)git checkout release
	$(SILENT)git pull
	$(SILENT)git merge master
	$(SILENT)git push
	$(SILENT)git checkout $(branch)
endif

# Note: linux only
deploy: test
	@mkdir build/bin
	@mkdir -p $(package)/usr/bin
	@mkdir -p $(package)/usr/include
	@mkdir -p $(package)/usr/lib
	@mkdir -p $(package)/usr/lib/pony/$(package_version)/bin
	@mkdir -p $(package)/usr/lib/pony/$(package_version)/include
	@mkdir -p $(package)/usr/lib/pony/$(package_version)/lib
	$(SILENT)cp build/release/libponyc.a $(package)/usr/lib/pony/$(package_version)/lib
	$(SILENT)cp build/release/libponyrt.a $(package)/usr/lib/pony/$(package_version)/lib
	$(SILENT)cp build/release/ponyc $(package)/usr/lib/pony/$(package_version)/bin
	$(SILENT)cp src/libponyrt/pony.h $(package)/usr/lib/pony/$(package_version)/include
	$(SILENT)ln -s /usr/lib/pony/$(package_version)/lib/libponyrt.a $(package)/usr/lib/libponyrt.a
	$(SILENT)ln -s /usr/lib/pony/$(package_version)/lib/libponyc.a $(package)/usr/lib/libponyc.a
	$(SILENT)ln -s /usr/lib/pony/$(package_version)/bin/ponyc $(package)/usr/bin/ponyc
	$(SILENT)ln -s /usr/lib/pony/$(package_version)/include/pony.h $(package)/usr/include/pony.h
	$(SILENT)cp -r packages $(package)/usr/lib/pony/$(package_version)/
	$(SILENT)build/release/ponyc packages/stdlib -rexpr -g -o $(package)/usr/lib/pony/$(package_version)
	$(SILENT)fpm -s dir -t deb -C $(package) -p build/bin --name ponyc --version $(package_version) --description "The Pony Compiler"
	$(SILENT)fpm -s dir -t rpm -C $(package) -p build/bin --name ponyc --version $(package_version) --description "The Pony Compiler"
	$(SILENT)git archive release > build/bin/$(archive)
	$(SILENT)cp -r $(package)/usr/lib/pony/$(package_version)/stdlib-docs stdlib-docs
	$(SILENT)tar rvf build/bin/$(archive) stdlib-docs
	$(SILENT)bzip2 build/bin/$(archive)
	$(SILENT)rm -rf $(package) build/bin/$(archive) stdlib-docs

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
	@echo '   valgrind'
	@echo '   pooltrack'
	@echo '   telemetry'
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
	@echo '  test              Run test suite'
	@echo '  install           Install ponyc'
	@echo '  uninstall         Remove all versions of ponyc'
	@echo '  stats             Print Pony cloc statistics'
	@echo '  clean             Delete all build files'
	@echo
