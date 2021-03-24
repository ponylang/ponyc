config ?= release
arch ?= native
tune ?= generic
build_flags ?= -j2
llvm_archs ?= X86;ARM;AArch64
llvm_config ?= Release
llc_arch ?= x86-64

ifndef version
  version := $(shell cat VERSION)
  ifneq ($(wildcard .git),)
    sha := $(shell git rev-parse --short HEAD)
    tag := $(version)-$(sha)
  else
    tag := $(version)
  endif
else
  tag := $(version)
endif

symlink := yes
ifdef DESTDIR
  prefix := $(DESTDIR)
  ponydir := $(prefix)
  symlink := no
else
  prefix ?= /usr/local
  ponydir ?= $(prefix)/lib/pony/$(tag)
endif

# Use clang by default; because CC defaults to 'cc'
# you must explicitly set CC=gcc to use gcc
ifndef CC
  ifneq (,$(shell clang --version 2>&1 | grep 'clang version'))
    CC = clang
    CXX = clang++
  endif
else ifeq ($(CC), cc)
  ifneq (,$(shell clang --version 2>&1 | grep 'clang version'))
    CC = clang
    CXX = clang++
  endif
endif

# By default, CC is cc and CXX is g++
# So if you use standard alternatives on many Linuxes
# You can get clang and g++ and then bad things will happen
ifneq (,$(shell $(CC) --version 2>&1 | grep 'clang version'))
  ifneq (,$(shell $(CXX) --version 2>&1 | grep "Free Software Foundation"))
    CXX = c++
  endif

  ifneq (,$(shell $(CXX) --version 2>&1 | grep "Free Software Foundation"))
    $(error CC is clang but CXX is g++. They must be from matching compilers.)
  endif
else ifneq (,$(shell $(CC) --version 2>&1 | grep "Free Software Foundation"))
  ifneq (,$(shell $(CXX) --version 2>&1 | grep 'clang version'))
    CXX = c++
  endif

  ifneq (,$(shell $(CXX) --version 2>&1 | grep 'clang version'))
    $(error CC is gcc but CXX is clang++. They must be from matching compilers.)
  endif
endif

srcDir := $(shell dirname '$(subst /Volumes/Macintosh HD/,/,$(realpath $(lastword $(MAKEFILE_LIST))))')
buildDir := $(srcDir)/build/build_$(config)
outDir := $(srcDir)/build/$(config)

libsSrcDir := $(srcDir)/lib
libsBuildDir := $(srcDir)/build/build_libs
libsOutDir := $(srcDir)/build/libs

ifndef verbose
  SILENT = @
  CMAKE_VERBOSE_FLAGS :=
else
  SILENT =
  CMAKE_VERBOSE_FLAGS := -DCMAKE_VERBOSE_MAKEFILE=ON
endif

CMAKE_FLAGS := $(CMAKE_FLAGS) $(CMAKE_VERBOSE_FLAGS)

ifeq ($(lto),yes)
  LTO_CONFIG_FLAGS = -DPONY_USE_LTO=true
else
  LTO_CONFIG_FLAGS =
endif

ifeq ($(runtime-bitcode),yes)
  ifeq (,$(shell $(CC) -v 2>&1 | grep clang))
    $(error Compiling the runtime as a bitcode file requires clang)
  endif
  BITCODE_FLAGS = -DPONY_RUNTIME_BITCODE=true
else
  BITCODE_FLAGS =
endif

PONY_USES =

comma:= ,
empty:=
space:= $(empty) $(empty)

define USE_CHECK
  $$(info Enabling use option: $1)
  ifeq ($1,valgrind)
    PONY_USES += -DPONY_USE_VALGRIND=true
  else ifeq ($1,thread_sanitizer)
    PONY_USES += -DPONY_USE_THREAD_SANITIZER=true
  else ifeq ($1,address_sanitizer)
    PONY_USES += -DPONY_USE_ADDRESS_SANITIZER=true
  else ifeq ($1,undefined_behavior_sanitizer)
    PONY_USES += -DPONY_USE_UNDEFINED_BEHAVIOR_SANITIZER=true
  else ifeq ($1,coverage)
    PONY_USES += -DPONY_USE_COVERAGE=true
  else ifeq ($1,pooltrack)
    PONY_USES += -DPONY_USE_POOLTRACK=true
  else ifeq ($1,dtrace)
    DTRACE ?= $(shell which dtrace)
    ifeq (, $$(DTRACE))
      $$(error No dtrace compatible user application static probe generation tool found)
    endif
    PONY_USES += -DPONY_USE_DTRACE=true
  else ifeq ($1,scheduler_scaling_pthreads)
    PONY_USES += -DPONY_USE_SCHEDULER_SCALING_PTHREADS=true
  else ifeq ($1,memtrack)
    PONY_USES += -DPONY_USE_MEMTRACK=true
  else ifeq ($1,memtrack_messages)
    PONY_USES += -DPONY_USE_MEMTRACK_MESSAGES=true
  else
    $$(error ERROR: Unknown use option specified: $1)
  endif
endef

ifdef use
  ifneq (${MAKECMDGOALS}, configure)
    $(error You can only specify use= for 'make configure')
	else
    $(foreach useitem,$(sort $(subst $(comma),$(space),$(use))),$(eval $(call USE_CHECK,$(useitem))))
  endif
endif

.DEFAULT_GOAL := build
.PHONY: all libs cleanlibs configure cross-configure build test test-ci test-check-version test-core test-stdlib-debug test-stdlib-release test-examples test-validate-grammar clean

libs:
	$(SILENT)mkdir -p '$(libsBuildDir)'
	$(SILENT)cd '$(libsBuildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake -B '$(libsBuildDir)' -S '$(libsSrcDir)' -DCMAKE_INSTALL_PREFIX="$(libsOutDir)" -DCMAKE_BUILD_TYPE="$(llvm_config)" -DLLVM_TARGETS_TO_BUILD="$(llvm_archs)" $(CMAKE_FLAGS)
	$(SILENT)cd '$(libsBuildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake --build '$(libsBuildDir)' --target install --config $(llvm_config) -- $(build_flags)

cleanlibs:
	$(SILENT)rm -rf '$(libsBuildDir)'
	$(SILENT)rm -rf '$(libsOutDir)'

configure:
	$(SILENT)mkdir -p '$(buildDir)'
	$(SILENT)cd '$(buildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake -B '$(buildDir)' -S '$(srcDir)' -DCMAKE_BUILD_TYPE=$(config) -DPONY_ARCH=$(arch) -DCMAKE_C_FLAGS="-march=$(arch) -mtune=$(tune)" -DCMAKE_CXX_FLAGS="-march=$(arch) -mtune=$(tune)" $(BITCODE_FLAGS) $(LTO_CONFIG_FLAGS) $(CMAKE_FLAGS) $(PONY_USES)

all: build

build:
	$(SILENT)cd '$(buildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake --build '$(buildDir)' --config $(config) --target all -- $(build_flags)

crossBuildDir := $(srcDir)/build/$(arch)/build_$(config)

cross-libponyrt:
	$(SILENT)mkdir -p $(crossBuildDir)
	$(SILENT)cd '$(crossBuildDir)' && env CC=$(CC) CXX=$(CXX) cmake -B '$(crossBuildDir)' -S '$(srcDir)' -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=$(arch) -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DPONY_CROSS_LIBPONYRT=true -DCMAKE_BUILD_TYPE=$(config) -DCMAKE_C_FLAGS="-march=$(arch) -mtune=$(tune)" -DCMAKE_CXX_FLAGS="-march=$(arch) -mtune=$(tune)" -DPONYC_VERSION=$(version) -DLL_FLAGS="-O3;-march=$(llc_arch);-mcpu=$(tune)"  $(CMAKE_FLAGS)
	$(SILENT)cd '$(crossBuildDir)' && env CC=$(CC) CXX=$(CXX) cmake --build '$(crossBuildDir)' --config $(config) --target libponyrt -- $(build_flags)

test: all test-core test-stdlib-release test-examples

test-ci: all test-check-version test-core test-stdlib-debug test-stdlib-release test-examples test-validate-grammar

test-cross-ci: cross_args=--triple=$(cross_triple) --cpu=$(cross_cpu) --link-arch=$(cross_arch) --linker='$(cross_linker)'
test-cross-ci: test-stdlib-debug test-stdlib-release

test-check-version: all
	$(SILENT)cd '$(outDir)' && ./ponyc --version

test-core: all
	$(SILENT)cd '$(outDir)' && ./libponyrt.tests --gtest_shuffle
	$(SILENT)cd '$(outDir)' && ./libponyc.tests --gtest_shuffle

test-stdlib-release: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc -b stdlib-release --pic --checktree --verify $(cross_args) ../../packages/stdlib && echo Built `pwd`/stdlib-release && $(cross_runner) ./stdlib-release --sequential && rm ./stdlib-release

test-stdlib-debug: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc -d -b stdlib-debug --pic --strip --checktree --verify $(cross_args) ../../packages/stdlib && echo Built `pwd`/stdlib-debug && $(cross_runner) ./stdlib-debug --sequential && rm ./stdlib-debug

test-examples: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) find ../../examples/*/* -name '*.pony' -print | xargs -n 1 dirname | sort -u | grep -v ffi- | xargs -n 1 -I {} ./ponyc -d -s --checktree -o {} {}

test-validate-grammar: all
	$(SILENT)cd '$(outDir)' && ./ponyc --antlr >> pony.g.new && diff ../../pony.g pony.g.new && rm pony.g.new

clean:
	$(SILENT)([ -d '$(buildDir)' ] && cd '$(buildDir)' && cmake --build '$(buildDir)' --config $(config) --target clean) || true
	$(SILENT)rm -rf $(crossBuildDir)
	$(SILENT)rm -rf $(buildDir)
	$(SILENT)rm -rf $(outDir)*

distclean:
	$(SILENT)([ -d build ] && rm -rf build) || true

install: build
	@mkdir -p $(ponydir)/bin
	@mkdir -p $(ponydir)/lib/$(arch)
	@mkdir -p $(ponydir)/include/pony/detail
	$(SILENT)cp $(buildDir)/src/libponyrt/libponyrt.a $(ponydir)/lib/$(arch)
	$(SILENT)if [ -f $(outDir)/libponyc.a ]; then cp $(outDir)/libponyc.a $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -f $(outDir)/libponyc-standalone.a ]; then cp $(outDir)/libponyc-standalone.a $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -f $(outDir)/libponyrt-pic.a ]; then cp $(outDir)/libponyrt-pic.a $(ponydir)/lib/$(arch); fi
	$(SILENT)cp $(outDir)/ponyc $(ponydir)/bin
	$(SILENT)cp src/libponyrt/pony.h $(ponydir)/include
	$(SILENT)cp src/common/pony/detail/atomics.h $(ponydir)/include/pony/detail
	$(SILENT)cp -r packages $(ponydir)/
ifeq ($(symlink),yes)
	@mkdir -p $(prefix)/bin
	@mkdir -p $(prefix)/lib
	@mkdir -p $(prefix)/include/pony/detail
	$(SILENT)ln -s -f $(ponydir)/bin/ponyc $(prefix)/bin/ponyc
	$(SILENT)if [ -f $(ponydir)/lib/$(arch)/libponyc.a ]; then ln -s -f $(ponydir)/lib/$(arch)/libponyc.a $(prefix)/lib/libponyc.a; fi
	$(SILENT)if [ -f $(ponydir)/lib/$(arch)/libponyc-standalone.a ]; then ln -s -f $(ponydir)/lib/$(arch)/libponyc-standalone.a $(prefix)/lib/libponyc-standalone.a; fi
	$(SILENT)if [ -f $(ponydir)/lib/$(arch)/libponyrt.a ]; then ln -s -f $(ponydir)/lib/$(arch)/libponyrt.a $(prefix)/lib/libponyrt.a; fi
	$(SILENT)if [ -f $(ponydir)/lib/$(arch)/libponyrt-pic.a ]; then ln -s -f $(ponydir)/lib/$(arch)/libponyrt-pic.a $(prefix)/lib/libponyrt-pic.a; fi
	$(SILENT)ln -s -f $(ponydir)/include/pony.h $(prefix)/include/pony.h
	$(SILENT)ln -s -f $(ponydir)/include/pony/detail/atomics.h $(prefix)/include/pony/detail/atomics.h
endif

uninstall:
	-$(SILENT)rm -rf $(ponydir) ||:
	-$(SILENT)rm -f $(prefix)/bin/ponyc ||:
	-$(SILENT)rm -f $(prefix)/lib/libponyc*.a ||:
	-$(SILENT)rm -f $(prefix)/lib/libponyrt*.a ||:
	-$(SILENT)rm -rf $(prefix)/lib/pony ||:
	-$(SILENT)rm -f $(prefix)/include/pony.h ||:
	-$(SILENT)rm -rf $(prefix)/include/pony ||:
