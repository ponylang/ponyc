# Determine the operating system
OSTYPE ?=

ifeq ($(OS),Windows_NT)
  OSTYPE = windows
else
  UNAME_S := $(shell uname -s)

  ifeq ($(UNAME_S),Linux)
    OSTYPE = linux
    lto := no

    ifneq (,$(shell which gcc-ar 2> /dev/null))
      AR = gcc-ar
      lto := yes
    endif

    ifdef LTO
      lto := yes
    endif
  endif

  ifeq ($(UNAME_S),Darwin)
    OSTYPE = osx
    lto := yes
  endif

  ifeq ($(UNAME_S),FreeBSD)
    OSTYPE = freebsd
    CXX = c++
    lto := no
  endif
endif

# Default settings (silent debug build).
config ?= debug
arch ?= native

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

prefix ?= /usr/local
destdir ?= $(prefix)/lib/pony/$(tag)

LIB_EXT ?= a
BUILD_FLAGS = -mcx16 -march=$(arch) -Werror -Wconversion \
  -Wno-sign-conversion -Wextra -Wall
LINKER_FLAGS = -mcx16 -march=$(arch)
AR_FLAGS =
ALL_CFLAGS = -std=gnu11 -DPONY_VERSION=\"$(tag)\" -DPONY_COMPILER=\"$(CC)\" -DPONY_ARCH=\"$(arch)\"
ALL_CXXFLAGS = -std=gnu++11 -fno-rtti

PONY_BUILD_DIR   ?= build/$(config)
PONY_SOURCE_DIR  ?= src
PONY_TEST_DIR ?= test
LLVM_FALLBACK := /usr/local/opt/llvm/bin/llvm-config

ifdef use
  ifneq (,$(filter $(use), valgrind))
    ALL_CFLAGS += -DUSE_VALGRIND
    PONY_BUILD_DIR := $(PONY_BUILD_DIR)-valgrind
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

    ifdef LTO
      AR_FLAGS += --plugin $(LTO)
    endif

    ifeq ($(OSTYPE),linux)
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
  ifneq (,$(shell which llvm-config 2> /dev/null))
    LLVM_CONFIG = llvm-config
  endif

  ifneq (,$(shell which llvm-config-3.6 2> /dev/null))
    LLVM_CONFIG = llvm-config-3.6
  endif

  ifneq (,$(shell which llvm-config36 2> /dev/null))
    LLVM_CONFIG = llvm-config36
  endif
endif

ifneq ("$(wildcard $(LLVM_FALLBACK))","")
  LLVM_CONFIG = $(LLVM_FALLBACK)
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
# (2) the linker flags necessary to link against the prebuilt library/libraries.
# (3) a list of include directories for a set of libraries.
# (4) a list of the libraries to link against.
llvm.ldflags := $(shell $(LLVM_CONFIG) --ldflags)
llvm.include := -isystem $(shell $(LLVM_CONFIG) --includedir)
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
libponyc.include := -I src/common/ -I src/libponyrt/ $(llvm.include)/
libponycc.include := -I src/common/ $(llvm.include)/
libponyrt.include := -I src/common/ -I src/libponyrt/
libponyrt-pic.include := $(libponyrt.include)

libponyc.tests.include := -I src/common/ -I src/libponyc/ -isystem lib/gtest/
libponyrt.tests.include := -I src/common/ -I src/libponyrt/ -isystem lib/gtest/

ponyc.include := -I src/common/ -I src/libponyrt/ $(llvm.include)/
libgtest.include := -isystem lib/gtest/

ifneq (,$(filter $(OSTYPE), osx freebsd))
  libponyrt.include += -I /usr/local/include
endif

# target specific build options
libponyc.buildoptions = -D__STDC_CONSTANT_MACROS
libponyc.buildoptions += -D__STDC_FORMAT_MACROS
libponyc.buildoptions += -D__STDC_LIMIT_MACROS

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

  ifeq ($(suffix $(1)),.c)
    compiler := $(CC)
    flags := $(ALL_CFLAGS)
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
	$(SILENT)$(AR) -rcs $$@ $(ofiles) $(AR_FLAGS)
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
	@cp $(PONY_BUILD_DIR)/libponyrt.a $(destdir)/lib
	@cp $(PONY_BUILD_DIR)/libponyc.a $(destdir)/lib
	@cp $(PONY_BUILD_DIR)/ponyc $(destdir)/bin
	@cp src/libponyrt/pony.h $(destdir)/include
	@cp -r packages $(destdir)/
ifeq ($$(symlink),yes)
	@mkdir -p $(prefix)/bin
	@mkdir -p $(prefix)/lib
	@mkdir -p $(prefix)/include
	@ln -sf $(destdir)/bin/ponyc $(prefix)/bin/ponyc
	@ln -sf $(destdir)/lib/libponyrt.a $(prefix)/lib/libponyrt.a
	@ln -sf $(destdir)/lib/libponyc.a $(prefix)/lib/libponyc.a
	@ln -sf $(destdir)/include/pony.h $(prefix)/include/pony.h
endif
endef

$(eval $(call EXPAND_INSTALL))

uninstall:
	-@rm -rf $(destdir) 2>/dev/null ||:
	-@rm $(prefix)/bin/ponyc 2>/dev/null ||:
	-@rm $(prefix)/lib/libponyrt.a 2>/dev/null ||:
	-@rm $(prefix)/lib/libponyc.a 2>/dev/null ||:
	-@rm $(prefix)/include/pony.h 2>/dev/null ||:

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
	@git add VERSION
	@git commit -m "Releasing version $(tag)"
	@git tag $(tag)
	@git push
	@git push --tags
	@git checkout release
	@git pull
	@git merge master
	@git push
	@git checkout $(branch)
endif

deploy: test
	@mkdir build/bin
	@mkdir -p $(package)/usr/bin
	@mkdir -p $(package)/usr/include
	@mkdir -p $(package)/usr/lib
	@mkdir -p $(package)/usr/lib/pony/$(package_version)/bin
	@mkdir -p $(package)/usr/lib/pony/$(package_version)/include
	@mkdir -p $(package)/usr/lib/pony/$(package_version)/lib
	@cp build/release/libponyc.a $(package)/usr/lib/pony/$(package_version)/lib
	@cp build/release/libponyrt.a $(package)/usr/lib/pony/$(package_version)/lib
	@cp build/release/ponyc $(package)/usr/lib/pony/$(package_version)/bin
	@cp src/libponyrt/pony.h $(package)/usr/lib/pony/$(package_version)/include
	@ln -s /usr/lib/pony/$(package_version)/lib/libponyrt.a $(package)/usr/lib/libponyrt.a
	@ln -s /usr/lib/pony/$(package_version)/lib/libponyc.a $(package)/usr/lib/libponyc.a
	@ln -s /usr/lib/pony/$(package_version)/bin/ponyc $(package)/usr/bin/ponyc
	@ln -s /usr/lib/pony/$(package_version)/include/pony.h $(package)/usr/include/pony.h
	@cp -r packages $(package)/usr/lib/pony/$(package_version)/
	@build/release/ponyc packages/stdlib -rexpr -g -o $(package)/usr/lib/pony/$(package_version)
	@fpm -s dir -t deb -C $(package) -p build/bin --name ponyc --version $(package_version) --description "The Pony Compiler"
	@fpm -s dir -t rpm -C $(package) -p build/bin --name ponyc --version $(package_version) --description "The Pony Compiler"
	@git archive release > build/bin/$(archive)
	@cp -r $(package)/usr/lib/pony/$(package_version)/stdlib-docs stdlib-docs
	@tar rvf build/bin/$(archive) stdlib-docs
	@bzip2 build/bin/$(archive)
	@rm -rf $(package) build/bin/$(archive) stdlib-docs

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
	@echo '  test              Run test suite'
	@echo '  install           Install ponyc'
	@echo '  uninstall         Remove all versions of ponyc'
	@echo '  stats             Print Pony cloc statistics'
	@echo '  clean             Delete all build files'
	@echo
