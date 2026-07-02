config ?= release
arch ?= native
cpu ?=
tune ?= generic
build_flags ?= -j2
llvm_archs ?= X86;ARM;AArch64;WebAssembly;RISCV
llvm_config ?= Release
llvm_tools ?= true
llc_arch ?= x86-64
pic_flag ?=
open_close_stress_connections ?= 10000000
test_full_program_timeout ?= 60

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

HOST_OS := $(shell uname -s)

# Make sure the compiler gets all relevant search paths on FreeBSD/OpenBSD/DragonFly
ifneq (,$(filter FreeBSD OpenBSD DragonFly,$(HOST_OS)))
  ifeq (,$(findstring /usr/local/include,$(shell echo $CPATH)))
    export CPATH = /usr/local/include:$CPATH
  endif
  ifeq (,$(findstring /usr/local/lib,$(shell echo $LIBRARY_PATH)))
    export LIBRARY_PATH = /usr/local/lib:$LIBRARY_PATH
  endif
endif

srcDir := $(shell dirname '$(subst /Volumes/Macintosh HD/,/,$(realpath $(lastword $(MAKEFILE_LIST))))')
buildDir := $(srcDir)/build/build_$(config)

# get outDir from CMake install file or fall back to default if the file doesn't exist
outDir := $(subst /libponyrt.tests,,$(shell grep -o -s '$(srcDir)/build/$(config).*/libponyrt.tests' $(buildDir)/cmake_install.cmake))
ifeq ($(outDir),)
  outDir := $(srcDir)/build/$(config)
endif

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

ifeq ($(filter $(llvm_tools),true false),)
  $(error llvm_tools must be 'true' or 'false', got '$(llvm_tools)')
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
    ifeq ($$(HOST_OS),OpenBSD)
      $$(error Valgrind is not supported on OpenBSD: Valgrind has no OpenBSD port, so its development headers aren't available. See BUILD.md)
    endif
    PONY_USES += -DPONY_USE_VALGRIND=true
  else ifeq ($1,thread_sanitizer)
    ifeq ($$(HOST_OS),OpenBSD)
      $$(error ThreadSanitizer is not supported on OpenBSD: the base toolchain ships no ThreadSanitizer runtime. See BUILD.md)
    endif
    ifeq ($$(HOST_OS),DragonFly)
      $$(error ThreadSanitizer is not supported on DragonFly: the gcc toolchain ships no ThreadSanitizer runtime. See BUILD.md)
    endif
    PONY_USES += -DPONY_USE_THREAD_SANITIZER=true
  else ifeq ($1,address_sanitizer)
    ifeq ($$(HOST_OS),OpenBSD)
      $$(error AddressSanitizer is not supported on OpenBSD: the base toolchain ships no AddressSanitizer runtime. See BUILD.md)
    endif
    ifeq ($$(HOST_OS),DragonFly)
      $$(error AddressSanitizer is not supported on DragonFly: the gcc toolchain ships no AddressSanitizer runtime. See BUILD.md)
    endif
    PONY_USES += -DPONY_USE_ADDRESS_SANITIZER=true
  else ifeq ($1,undefined_behavior_sanitizer)
    ifeq ($$(HOST_OS),OpenBSD)
      $$(error UndefinedBehaviorSanitizer is not supported on OpenBSD: the base toolchain ships only the minimal UBSan runtime, not the standalone runtime ponyc links against. See BUILD.md)
    endif
    ifeq ($$(HOST_OS),DragonFly)
      $$(error UndefinedBehaviorSanitizer is not supported on DragonFly: the gcc toolchain ships no UndefinedBehaviorSanitizer runtime. See BUILD.md)
    endif
    PONY_USES += -DPONY_USE_UNDEFINED_BEHAVIOR_SANITIZER=true
  else ifeq ($1,coverage)
    ifeq ($$(HOST_OS),OpenBSD)
      $$(error Coverage instrumentation is not supported on OpenBSD: the base toolchain's profiling runtime is incomplete, so coverage builds fail to link. See BUILD.md)
    endif
    PONY_USES += -DPONY_USE_COVERAGE=true
  else ifeq ($1,pooltrack)
    PONY_USES += -DPONY_USE_POOLTRACK=true
  else ifeq ($1,dtrace)
    ifeq ($$(HOST_OS),OpenBSD)
      $$(error DTrace is not supported on OpenBSD. See BUILD.md)
    endif
    ifeq ($$(HOST_OS),DragonFly)
      $$(error DTrace is not supported on DragonFly. See BUILD.md)
    endif
    DTRACE ?= $(shell which dtrace)
    ifeq (, $$(DTRACE))
      $$(error No dtrace compatible user application static probe generation tool found)
    endif
    PONY_USES += -DPONY_USE_DTRACE=true
  else ifeq ($1,scheduler_scaling_pthreads)
    PONY_USES += -DPONY_USE_SCHEDULER_SCALING_PTHREADS=true
  else ifeq ($1,systematic_testing)
    PONY_USES += -DPONY_USE_SYSTEMATIC_TESTING=true
  else ifeq ($1,runtimestats)
    PONY_USES += -DPONY_USE_RUNTIMESTATS=true
  else ifeq ($1,runtimestats_messages)
    PONY_USES += -DPONY_USE_RUNTIMESTATS_MESSAGES=true
  else ifeq ($1,pool_memalign)
    PONY_USES += -DPONY_USE_POOL_MEMALIGN=true
  else ifeq ($1,pool_retain)
    PONY_USES += -DPONY_USE_POOL_RETAIN=true
  else ifeq ($1,runtime_tracing)
    PONY_USES += -DPONY_USE_RUNTIME_TRACING=true
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

# The wrapper (batch flags, signal pass-lists, on-crash commands) lives in
# .ci-scripts/run-under-debugger.bash so it belongs to the CI process rather
# than this Makefile.
ifneq ($(findstring lldb,$(usedebugger)),)
  debuggercmd := $(srcDir)/.ci-scripts/run-under-debugger.bash $(usedebugger)
  testextras := --gtest_throw_on_failure
else ifneq ($(findstring gdb,$(usedebugger)),)
  debuggercmd := $(srcDir)/.ci-scripts/run-under-debugger.bash $(usedebugger)
else ifneq ($(strip $(usedebugger)),)
  $(error Unknown debugger: '$(usedebugger)')
endif

.DEFAULT_GOAL := build
.PHONY: all libs cleanlibs configure cross-configure build test test-ci-core test-check-version test-core test-stdlib-debug test-stdlib-release test-examples test-stress test-validate-grammar clean test-pony-lsp pony-lint test-pony-lint lint-pony-lint lint-pony-doc lint-pony-lsp pony-doc test-pony-doc test-pony-compiler

libs:
	$(SILENT)mkdir -p '$(libsBuildDir)'
	$(SILENT)cd '$(libsBuildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake -B '$(libsBuildDir)' -S '$(libsSrcDir)' -DPONY_PIC_FLAG=$(pic_flag) -DCMAKE_INSTALL_PREFIX="$(libsOutDir)" -DCMAKE_BUILD_TYPE="$(llvm_config)" -DLLVM_TARGETS_TO_BUILD="$(llvm_archs)" -DPONY_LLVM_TOOLS=$(llvm_tools) $(CMAKE_FLAGS)
	$(SILENT)cd '$(libsBuildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake --build '$(libsBuildDir)' --target install --config $(llvm_config) -- $(build_flags)

cleanlibs:
	$(SILENT)rm -rf '$(libsBuildDir)'
	$(SILENT)rm -rf '$(libsOutDir)'

configure:
	$(SILENT)mkdir -p '$(buildDir)'
	$(SILENT)cd '$(buildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake -B '$(buildDir)' -S '$(srcDir)' -DCMAKE_BUILD_TYPE=$(config) -DPONY_ARCH=$(arch) -DPONY_CPU=$(cpu) -DPONY_PIC_FLAG=$(pic_flag) -DPONYC_VERSION=$(tag) -DCMAKE_C_FLAGS="-march=$(arch) -mtune=$(tune)" -DCMAKE_CXX_FLAGS="-march=$(arch) -mtune=$(tune)" $(BITCODE_FLAGS) $(LTO_CONFIG_FLAGS) $(CMAKE_FLAGS) $(PONY_USES)

all: build

build:
	$(SILENT)cd '$(buildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake --build '$(buildDir)' --config $(config) --target all -- $(build_flags)

ponyc:
	$(SILENT)cd '$(buildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake --build '$(buildDir)' --config $(config) --target ponyc -- $(build_flags)

pony-lsp:
	$(SILENT)cd '$(buildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake --build '$(buildDir)' --config $(config) --target tools.pony-lsp -- $(build_flags)

pony-lint:
	$(SILENT)cd '$(buildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake --build '$(buildDir)' --config $(config) --target tools.pony-lint -- $(build_flags)

pony-doc:
	$(SILENT)cd '$(buildDir)' && env CC="$(CC)" CXX="$(CXX)" cmake --build '$(buildDir)' --config $(config) --target tools.pony-doc -- $(build_flags)

crossBuildDir := $(srcDir)/build/$(arch)/build_$(config)

cross-libponyrt:
	$(SILENT)mkdir -p $(crossBuildDir)
	$(SILENT)cd '$(crossBuildDir)' && env CC=$(CC) CXX=$(CXX) cmake -B '$(crossBuildDir)' -S '$(srcDir)' -DCMAKE_CROSSCOMPILING=true -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=$(arch) -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DPONY_CROSS_LIBPONYRT=true -DCMAKE_BUILD_TYPE=$(config) -DCMAKE_C_FLAGS="$(cross_cflags)" -DCMAKE_CXX_FLAGS="$(cross_cflags)" -DPONY_ARCH=$(arch) -DPONYC_VERSION=$(version) -DLL_FLAGS="$(cross_llc_flags)"  $(CMAKE_FLAGS)
	$(SILENT)cd '$(crossBuildDir)' && env CC=$(CC) CXX=$(CXX) cmake --build '$(crossBuildDir)' --config $(config) --target libponyrt crt_objects -- $(build_flags)

test: all test-core test-stdlib-release test-examples

test-ci-core: all test-check-version test-core test-stdlib-debug test-stdlib-release test-examples test-validate-grammar

test-cross-ci: cross_args=--triple=$(cross_triple) --cpu=$(cross_cpu) --link-arch=$(cross_arch) $(if $(cross_sysroot),--sysroot='$(cross_sysroot)') $(cross_ponyc_args)
test-cross-ci: debuggercmd=
test-cross-ci: test-core test-stdlib-debug test-stdlib-release test-examples

test-check-version: all
	$(SILENT)cd '$(outDir)' && ./ponyc --version

test-core: all test-libponyrt test-libponyc test-full-programs-debug test-full-programs-release

test-libponyrt: all
	$(SILENT)cd '$(outDir)' && $(debuggercmd) ./libponyrt.tests --gtest_shuffle $(testextras)

test-libponyc: all
	$(SILENT)cd '$(outDir)' && $(debuggercmd) ./libponyc.tests --gtest_shuffle $(testextras)

ifneq (,$(filter FreeBSD OpenBSD DragonFly Darwin,$(HOST_OS)))
  num_cores := `sysctl -n hw.ncpu`
else
  num_cores := `nproc --all`
endif

test-full-programs-release: all
	@mkdir -p $(outDir)/full-program-tests/release
	$(SILENT)cd '$(outDir)' && $(buildDir)/test/full-program-runner/full-program-runner --debugger='$(debuggercmd)' --timeout_s=$(test_full_program_timeout) --max_parallel=1 --compiler=$(outDir)/ponyc --output=$(outDir)/full-program-tests/release --test_lib=$(outDir)/test_lib $(srcDir)/test/full-program-tests

test-full-programs-debug: all
	@mkdir -p $(outDir)/full-program-tests/debug
	$(SILENT)cd '$(outDir)' && $(buildDir)/test/full-program-runner/full-program-runner --debugger='$(debuggercmd)' --timeout_s=$(test_full_program_timeout) --max_parallel=1 --compiler=$(outDir)/ponyc --debug --output=$(outDir)/full-program-tests/debug --test_lib=$(outDir)/test_lib $(srcDir)/test/full-program-tests

# stdlib_test_excludes is appended to the stdlib test runner invocation and is
# empty by default. Set it on the make command line (e.g.
# stdlib_test_excludes='--exclude=net/Broadcast') to skip environment-sensitive
# tests on a specific CI leg without affecting any other leg. See the riscv64
# job in .github/workflows/ponyc-tier3.yml.
test-stdlib-release: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc -b stdlib-release --pic --checktree $(cross_args) ../../packages/stdlib && echo Built `pwd`/stdlib-release && $(cross_runner) $(debuggercmd) ./stdlib-release --sequential $(stdlib_test_excludes)

test-stdlib-debug: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc -d -b stdlib-debug --pic --strip --checktree $(cross_args) ../../packages/stdlib && echo Built `pwd`/stdlib-debug && $(cross_runner) $(debuggercmd) ./stdlib-debug --sequential $(stdlib_test_excludes)

test-examples: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) find ../../examples/*/* -name '*.pony' -print | xargs -n 1 dirname | sort -u | grep -v ffi- | xargs -n 1 -I {} ./ponyc -d -s --checktree -o {} {}

test-validate-grammar: all
	$(SILENT)cd '$(outDir)' && ./ponyc --antlr > pony.g.new && diff ../../pony.g pony.g.new

# File-target rules below provide incremental rebuild for the five
# Pony-source binaries this Makefile builds outside cmake:
# `pony-lint-ci` and the four `pony-*-tests` test binaries.
# Order-only `| all` lets cmake build and refresh ponyc without
# forcing the file rule stale on every invocation. Order-only doesn't
# propagate mtime, so each rule also carries regular prereqs on
# `ponyc-bin-srcs` and `stdlib-srcs` -- otherwise
# `touch src/foo.c && make test-pony-doc` would miss the rebuild.
# Directory lists are prereqs alongside file lists so `find` notices
# file deletions (a directory's mtime updates on add/remove; surviving
# files' mtimes don't move).

# pony_compiler library -- shared by pony-lint-ci and every
# pony-*-tests binary.
compiler-lib-srcs := $(shell find $(srcDir)/tools/lib/ponylang/pony_compiler/pony_compiler -name '*.pony' -not -name '.*')
compiler-lib-dirs := $(shell find $(srcDir)/tools/lib/ponylang/pony_compiler/pony_compiler -type d -not -name '.*')

# pony-lint-ci sources -- the lint binary shared by all three
# lint-pony-* targets. Sources are the lint tool minus its test/
# sub-package; the pruned subtree is the entry for pony-lint-tests
# below, where it is included rather than pruned.
lint-bin-srcs := $(shell find $(srcDir)/tools/pony-lint -path $(srcDir)/tools/pony-lint/test -prune -o -name '*.pony' -not -name '.*' -print) \
                 $(compiler-lib-srcs)
lint-bin-dirs := $(shell find $(srcDir)/tools/pony-lint -path $(srcDir)/tools/pony-lint/test -prune -o -type d -not -name '.*' -print) \
                 $(compiler-lib-dirs)

# pony-doc-tests / pony-lint-tests sources -- each binary builds
# tools/<tool>/test, and the test code uses ".." to pull the parent
# package source in. Recursive find covers both. We don't prune the
# test entry (unlike lint-bin-srcs above) -- for the test binary, the
# test entry IS the build entry.
doc-test-srcs := $(shell find $(srcDir)/tools/pony-doc -name '*.pony' -not -name '.*')
doc-test-dirs := $(shell find $(srcDir)/tools/pony-doc -type d -not -name '.*')
lint-test-srcs := $(shell find $(srcDir)/tools/pony-lint -name '*.pony' -not -name '.*')
lint-test-dirs := $(shell find $(srcDir)/tools/pony-lint -type d -not -name '.*')

# pony-compiler-tests sources -- builds tools/.../tests; the test code
# uses "../pony_compiler" so the library comes in via compiler-lib-*
# in the file-target rule. Recursive find also sweeps in runtime
# fixtures (compile_errors_*/, simple/, constructs/) loaded at
# runtime, not compile time. Editing a fixture triggers an
# unnecessary rebuild -- accepted trade-off (simpler rule; a future
# contributor adding a real `use`d sub-package gets correct tracking
# for free).
compiler-test-srcs := $(shell find $(srcDir)/tools/lib/ponylang/pony_compiler/tests -name '*.pony' -not -name '.*')
compiler-test-dirs := $(shell find $(srcDir)/tools/lib/ponylang/pony_compiler/tests -type d -not -name '.*')

# pony-lsp-tests sources -- the build entry is tools/, which compiles
# tools/_test.pony and transitively pulls tools/pony-lsp via
# `use lsp = "pony-lsp/test"`. -maxdepth 1 picks up tools/_test.pony
# without recursing into pony-doc/ or pony-lint/ (which the lsp build
# does not compile). Listing $(srcDir)/tools as a directory prereq
# means top-of-tools/ adds invalidate this binary; acceptable since
# tools/ rarely gains files. Recursive find sweeps in runtime fixtures
# under test/workspace/<feature>/ and test/error_workspace/ -- same
# trade-off as compiler-test-srcs above.
lsp-test-srcs := $(shell find $(srcDir)/tools -maxdepth 1 -name '*.pony' -not -name '.*') \
                 $(shell find $(srcDir)/tools/pony-lsp -name '*.pony' -not -name '.*')
lsp-test-dirs := $(srcDir)/tools \
                 $(shell find $(srcDir)/tools/pony-lsp -type d -not -name '.*')

ponyc-bin-srcs := $(shell find $(srcDir)/src -type f \( -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.hh' -o -name '*.ll' -o -name '*.d' \) -not -name '.*')
ponyc-bin-dirs := $(shell find $(srcDir)/src -type d -not -name '.*')
stdlib-srcs := $(shell find $(srcDir)/packages -name '*.pony' -not -name '.*')
stdlib-dirs := $(shell find $(srcDir)/packages -type d -not -name '.*')

# Empty recipe so $(outDir)/ponyc can be a regular prereq below
# without "No rule to make target" on a clean checkout -- `all`
# produces it as a side effect via cmake.
$(outDir)/ponyc: | all ;

$(outDir)/pony-lint-ci: $(lint-bin-srcs) $(lint-bin-dirs) $(ponyc-bin-srcs) $(ponyc-bin-dirs) $(stdlib-srcs) $(stdlib-dirs) $(outDir)/ponyc | all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc --path ../../tools/lib/ponylang/pony_compiler/ -b pony-lint-ci ../../tools/pony-lint && echo Built `pwd`/pony-lint-ci

lint-pony-lint: $(outDir)/pony-lint-ci
	$(SILENT)cd '$(outDir)' && PONYPATH=../../tools/lib/ponylang/pony_compiler:$(PONYPATH) ./pony-lint-ci ../../tools/pony-lint/

lint-pony-doc: $(outDir)/pony-lint-ci
	$(SILENT)cd '$(outDir)' && PONYPATH=../../tools/lib/ponylang/pony_compiler:$(PONYPATH) ./pony-lint-ci ../../tools/pony-doc/

lint-pony-lsp: $(outDir)/pony-lint-ci
	$(SILENT)cd '$(outDir)' && PONYPATH=../../tools/lib/ponylang/pony_compiler:$(PONYPATH) ./pony-lint-ci ../../tools/pony-lsp/

$(outDir)/pony-doc-tests: $(doc-test-srcs) $(doc-test-dirs) $(compiler-lib-srcs) $(compiler-lib-dirs) $(ponyc-bin-srcs) $(ponyc-bin-dirs) $(stdlib-srcs) $(stdlib-dirs) $(outDir)/ponyc | all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc --path ../../tools/lib/ponylang/pony_compiler/ -b pony-doc-tests ../../tools/pony-doc/test && echo Built `pwd`/pony-doc-tests

$(outDir)/pony-lsp-tests: $(lsp-test-srcs) $(lsp-test-dirs) $(compiler-lib-srcs) $(compiler-lib-dirs) $(ponyc-bin-srcs) $(ponyc-bin-dirs) $(stdlib-srcs) $(stdlib-dirs) $(outDir)/ponyc | all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc --path ../../tools/lib/ponylang/pony_compiler/ -b pony-lsp-tests ../../tools && echo Built `pwd`/pony-lsp-tests

$(outDir)/pony-lint-tests: $(lint-test-srcs) $(lint-test-dirs) $(compiler-lib-srcs) $(compiler-lib-dirs) $(ponyc-bin-srcs) $(ponyc-bin-dirs) $(stdlib-srcs) $(stdlib-dirs) $(outDir)/ponyc | all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc --path ../../tools/lib/ponylang/pony_compiler/ -b pony-lint-tests ../../tools/pony-lint/test && echo Built `pwd`/pony-lint-tests

$(outDir)/pony-compiler-tests: $(compiler-test-srcs) $(compiler-test-dirs) $(compiler-lib-srcs) $(compiler-lib-dirs) $(ponyc-bin-srcs) $(ponyc-bin-dirs) $(stdlib-srcs) $(stdlib-dirs) $(outDir)/ponyc | all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc --path ../../tools/lib/ponylang/pony_compiler/ -b pony-compiler-tests ../../tools/lib/ponylang/pony_compiler/tests && echo Built `pwd`/pony-compiler-tests

test-pony-doc: $(outDir)/pony-doc-tests
	$(SILENT)cd '$(outDir)' && ./pony-doc-tests --sequential

test-pony-lsp: $(outDir)/pony-lsp-tests
	$(SILENT)cd '$(outDir)' && ./pony-lsp-tests --sequential

test-pony-lint: $(outDir)/pony-lint-tests
	$(SILENT)cd '$(outDir)' && PONYPATH=../../packages:$(PONYPATH) ./pony-lint-tests --sequential

test-pony-compiler: $(outDir)/pony-compiler-tests
	$(SILENT)cd '$(outDir)' && PONYPATH=../../tools/lib/ponylang/pony_compiler:../../packages:$(PONYPATH) ./pony-compiler-tests --sequential

test-cross-stress-release: cross_args=--triple=$(cross_triple) --cpu=$(cross_cpu) --link-arch=$(cross_arch) $(if $(cross_sysroot),--sysroot='$(cross_sysroot)') $(cross_ponyc_args)
test-cross-stress-release: debuggercmd=
test-cross-stress-release: test-stress-release
test-stress-tcp-open-close-release: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc -b open-close --pic $(cross_args) ../../test/rt-stress/tcp-open-close && echo Built `pwd`/open-close && $(cross_runner) $(debuggercmd) ./open-close --ponynoblock $(open_close_stress_connections)

test-cross-stress-debug: cross_args=--triple=$(cross_triple) --cpu=$(cross_cpu) --link-arch=$(cross_arch) $(if $(cross_sysroot),--sysroot='$(cross_sysroot)') $(cross_ponyc_args)
test-cross-stress-debug: debuggercmd=
test-cross-stress-debug: test-stress-debug
test-stress-tcp-open-close-debug: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc -d -b open-close --pic $(cross_args) ../../test/rt-stress/tcp-open-close && echo Built `pwd`/open-close && $(cross_runner) $(debuggercmd) ./open-close --ponynoblock $(open_close_stress_connections)

test-cross-stress-with-cd-release: cross_args=--triple=$(cross_triple) --cpu=$(cross_cpu) --link-arch=$(cross_arch) $(if $(cross_sysroot),--sysroot='$(cross_sysroot)') $(cross_ponyc_args)
test-cross-stress-with-cd-release: debuggercmd=
test-cross-stress-with-cd-release: test-stress-with-cd-release
test-stress-tcp-open-close-with-cd-release: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc -b open-close --pic $(cross_args) ../../test/rt-stress/tcp-open-close && echo Built `pwd`/open-close && $(cross_runner) $(debuggercmd) ./open-close $(open_close_stress_connections)

test-cross-stress-with-cd-debug: cross_args=--triple=$(cross_triple) --cpu=$(cross_cpu) --link-arch=$(cross_arch) $(if $(cross_sysroot),--sysroot='$(cross_sysroot)') $(cross_ponyc_args)
test-cross-stress-with-cd-debug: debuggercmd=
test-cross-stress-with-cd-debug: test-stress-with-cd-debug
test-stress-tcp-open-close-with-cd-debug: all
	$(SILENT)cd '$(outDir)' && PONYPATH=.:$(PONYPATH) ./ponyc -d -b open-close --pic $(cross_args) ../../test/rt-stress/tcp-open-close && echo Built `pwd`/open-close && $(cross_runner) $(debuggercmd) ./open-close $(open_close_stress_connections)

clean:
	$(SILENT)([ -d '$(buildDir)' ] && cd '$(buildDir)' && cmake --build '$(buildDir)' --config $(config) --target clean) || true
	$(SILENT)rm -rf $(crossBuildDir)
	$(SILENT)rm -rf $(buildDir)
	$(SILENT)rm -rf $(outDir)*

distclean:
	$(SILENT)([ -d build ] && rm -rf build) || true

# Shared-dir symlinks that older ponyc installs created but current ones no
# longer do: the runtime headers and runtime static libs that only supported
# hand-embedding the runtime. Both install (in-place upgrade) and uninstall
# remove these, targeted so they only ever remove our own symlinks — a symlink
# whose target is under $(prefix)/lib/pony — never a user's real file. Only the
# default ponydir layout is matched; an old install done with a custom ponydir
# isn't auto-cleaned. Paths are relative to $(prefix). Transitional: this
# cleanup moves to the CMake install when the Makefile is retired (#5588) and
# stays for a while, until installs with the old layout are gone, then it's
# removed.
legacy-embed-symlinks := include/pony.h include/ponyassert.h include/platform.h include/threads.h include/paths.h include/pony/detail/atomics.h lib/libponyrt.a lib/libponyrt-pic.a lib/libdtrace_probes.a

# Canned recipe shared by install and uninstall. The case-glob on readlink's
# target is an anchored prefix match, so a user's symlink whose target merely
# contains "$(prefix)/lib/pony/" elsewhere is left alone; the [ -L ] guard
# spares a user's real file. readlink still matches our dangling symlinks after
# uninstall removes $(prefix)/lib/pony first. The trailing `|| :` keeps a
# permission error on the final rm from aborting the recipe.
define clean-legacy-embed-symlinks
$(SILENT)for f in $(legacy-embed-symlinks); do l="$(prefix)/$$f"; if [ -L "$$l" ]; then case "$$(readlink "$$l")" in "$(prefix)/lib/pony/"*) rm -f "$$l" ;; esac; fi; done || :
-$(SILENT)rmdir "$(prefix)/include/pony/detail" "$(prefix)/include/pony" 2>/dev/null || :
endef

install: build
	@mkdir -p $(ponydir)/bin
	@mkdir -p $(ponydir)/lib/$(arch)
	@mkdir -p $(ponydir)/include/pony/detail
	$(SILENT)cp $(buildDir)/src/libponyrt/libponyrt.a $(ponydir)/lib/$(arch)
	$(SILENT)if [ -f $(buildDir)/src/libponyrt/libdtrace_probes.a ]; then cp $(buildDir)/src/libponyrt/libdtrace_probes.a $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -f $(outDir)/libponyc.a ]; then cp $(outDir)/libponyc.a $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -f $(outDir)/libponyc-standalone.a ]; then cp $(outDir)/libponyc-standalone.a $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -f $(outDir)/libponyrt-pic.a ]; then cp $(outDir)/libponyrt-pic.a $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -f $(outDir)/crtbeginS.o ]; then cp $(outDir)/crtbeginS.o $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -f $(outDir)/crtendS.o ]; then cp $(outDir)/crtendS.o $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -f $(outDir)/crtbeginT.o ]; then cp $(outDir)/crtbeginT.o $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -f $(outDir)/crtend.o ]; then cp $(outDir)/crtend.o $(ponydir)/lib/$(arch); fi
	$(SILENT)if [ -d $(libsOutDir)/lib/clang ]; then cp -r $(libsOutDir)/lib/clang $(ponydir)/lib/; fi
	$(SILENT)if [ -d $(libsOutDir)/lib64/clang ]; then cp -r $(libsOutDir)/lib64/clang $(ponydir)/lib/; fi
	$(SILENT)cp $(outDir)/ponyc $(ponydir)/bin
	$(SILENT)if [ -f $(outDir)/pony-lsp ]; then cp $(outDir)/pony-lsp $(ponydir)/bin; fi
	$(SILENT)if [ -f $(outDir)/pony-lint ]; then cp $(outDir)/pony-lint $(ponydir)/bin; fi
	$(SILENT)if [ -f $(outDir)/pony-doc ]; then cp $(outDir)/pony-doc $(ponydir)/bin; fi
	$(SILENT)cp src/libponyrt/pony.h $(ponydir)/include/pony
	$(SILENT)cp src/common/pony/detail/atomics.h $(ponydir)/include/pony/detail
# The C-shim compiler finds these relative to the ponyc binary (gencshim
# add_pony_include_args); they are not installed into the shared $(prefix)
# dirs. pony.h and pony_assert (ponyassert.h) are supported for shims;
# platform.h/threads.h/paths.h are ponyassert.h's internal include closure,
# not a committed interface. Kept under pony/ so that when this dir is a shared
# system include (DESTDIR packaging, Windows) no flat collision-prone name
# (paths.h, threads.h) lands there. vcvars.h is Windows-only, shipped by the
# cmake install, not here. See .known-couplings/clang-resource-dir.md.
	$(SILENT)cp src/common/ponyassert.h $(ponydir)/include/pony
	$(SILENT)cp src/common/platform.h $(ponydir)/include/pony
	$(SILENT)cp src/common/threads.h $(ponydir)/include/pony
	$(SILENT)cp src/common/paths.h $(ponydir)/include/pony
	$(SILENT)cp -r packages $(ponydir)/
ifeq ($(symlink),yes)
	@mkdir -p $(prefix)/bin
	@mkdir -p $(prefix)/lib
	$(SILENT)ln -s -f $(ponydir)/bin/ponyc $(prefix)/bin/ponyc
	$(SILENT)if [ -f $(ponydir)/bin/pony-lsp ]; then ln -s -f $(ponydir)/bin/pony-lsp $(prefix)/bin/pony-lsp; fi
	$(SILENT)if [ -f $(ponydir)/bin/pony-lint ]; then ln -s -f $(ponydir)/bin/pony-lint $(prefix)/bin/pony-lint; fi
	$(SILENT)if [ -f $(ponydir)/bin/pony-doc ]; then ln -s -f $(ponydir)/bin/pony-doc $(prefix)/bin/pony-doc; fi
	$(SILENT)if [ -f $(ponydir)/lib/$(arch)/libponyc.a ]; then ln -s -f $(ponydir)/lib/$(arch)/libponyc.a $(prefix)/lib/libponyc.a; fi
	$(SILENT)if [ -f $(ponydir)/lib/$(arch)/libponyc-standalone.a ]; then ln -s -f $(ponydir)/lib/$(arch)/libponyc-standalone.a $(prefix)/lib/libponyc-standalone.a; fi
# ponyc no longer installs its runtime headers or runtime static libs into the
# shared $(prefix) dirs (the C-shim compiler reads headers from $(ponydir)
# relative to the binary). Clean up the symlinks an older install left here.
	$(clean-legacy-embed-symlinks)
endif

uninstall:
	-$(SILENT)rm -rf $(ponydir) ||:
	-$(SILENT)rm -f $(prefix)/bin/ponyc ||:
	-$(SILENT)rm -f $(prefix)/bin/pony-lsp ||:
	-$(SILENT)rm -f $(prefix)/bin/pony-lint ||:
	-$(SILENT)rm -f $(prefix)/bin/pony-doc ||:
	-$(SILENT)rm -f $(prefix)/lib/libponyc*.a ||:
	-$(SILENT)rm -rf $(prefix)/lib/pony ||:
# Runtime headers/libs an older install symlinked into the shared dirs (current
# installs don't create these).
	$(clean-legacy-embed-symlinks)
