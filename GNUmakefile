# This GNUmakefile optionally invokes lib/llvm/Makefile if use=pony_lib_llvm is
# provided on the command line and then compiles ponyc by invoking Makefile.
# The version of llvm can be controlled by passing LLVM_PROJ on the command
# line. The default is llvm 3.9.1, passing LLVM_PROJ=tags/RELEASE_600/final will
# cause llvm 6.0.0 to be compiled.

ROOT_DIR=$(shell pwd)
llvm_dir=$(ROOT_DIR)/lib/llvm
LLVM_PROJ=tags/RELEASE_391/final

ifneq (,$(filter $(use), pony_lib_llvm))
  PONY_LIB_LLVM=$(llvm_dir)
  LLVM_CONFIG=$(PONY_LIB_LLVM)/dist/bin/llvm-config
  NEW_PATH=$(PONY_LIB_LLVM)/dist/bin:$(PATH)
  pre_targets=$(LLVM_CONFIG)
else
  NEW_PATH=$(PATH)
  pre_targets=
endif

pony_targets = libponyc libponyrt libponyrt-pic libponyc.tests libponyrt.tests libponyc.benchmarks
pony_targets += libponyrt.benchmarks ponyc benchmark install uninstall stats test all
pony_targets += stdlib test-stdlib stdlib-debug test-stdlib-debug test-examples test-ci docs-online


.PHONY: $(pony_targets)
$(pony_targets): $(pre_targets)
	@PATH=$(NEW_PATH) $(MAKE) -f Makefile $(MAKECMDGOALS) $(MAKEFLAGS)

$(PONY_LIB_LLVM)/dist/bin/llvm-config:
	@$(MAKE) -C $(PONY_LIB_LLVM) LLVM_PROJ=$(LLVM_PROJ)
	@$(MAKE) -C $(PONY_LIB_LLVM) install

clean:
	@$(MAKE) -f Makefile clean

clean-all: clean
	@$(MAKE) -C lib/llvm clean

distclean: clean-all
	@$(MAKE) -C lib/llvm distclean

help:
	@echo 'Usage: make [config=name] [arch=name] [use=opt,...] [target]'
	@echo
	@echo 'CONFIGURATIONS:'
	@echo '  debug'
	@echo '  release (default)'
	@echo
	@echo 'ARCHITECTURE:'
	@echo '  native (default)'
	@echo '  [any compiler supported architecture]'
	@echo
	@echo 'Compile time default options:'
	@echo '  default_pic=true     Make --pic the default'
	@echo '  default_ssl=Name     Make Name the default ssl version'
	@echo '                       where Name is one of:'
	@echo '                         openssl_0.9.0'
	@echo '                         openssl_1.1.0'
	@echo '  LLVM_PROJ=Tag        Make llvm the default Tag is tags/RELEASE_391/final'
	@echo '                       other Tag values, such as tags/RELEASE_600/final'
	@echo '                       are possible.'
	@echo
	@echo 'USE OPTIONS:'
	@echo '   valgrind'
	@echo '   pooltrack'
	@echo '   dtrace'
	@echo '   actor_continuations'
	@echo '   coverage'
	@echo '   scheduler_scaling_pthreads'
	@echo '   llvm_link_static'
	@echo '   pony_lib_llvm'
	@echo
	@echo 'TARGETS:'
	@echo '  libponyc               Pony compiler library'
	@echo '  libponyrt              Pony runtime'
	@echo '  libponyrt-pic          Pony runtime -fpic'
	@echo '  libponyc.tests         Test suite for libponyc'
	@echo '  libponyrt.tests        Test suite for libponyrt'
	@echo '  libponyc.benchmarks    Benchmark suite for libponyc'
	@echo '  libponyrt.benchmarks   Benchmark suite for libponyrt'
	@echo '  ponyc                  Pony compiler executable'
	@echo
	@echo '  all                    Build all of the above (default)'
	@echo '  test                   Run test suite'
	@echo '  benchmark              Build and run benchmark suite'
	@echo '  install                Install ponyc'
	@echo '  uninstall              Remove all versions of ponyc'
	@echo '  stats                  Print Pony cloc statistics'
	@echo '  clean                  Delete all build files but nothing in $(llvm_dir)'
	@echo '  clean-all              clean plus clean $(llvm_dir)'
	@echo '  distclean              clean-all plus distclean $(llvm_dir)'
	@echo
