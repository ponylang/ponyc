# Since the current pony Makefile requires llvm-config to exist before
# its run GNUmakefile will compile lib/llvm before invoking Makefile.
# This works because gnu make searchs for GNUmakefile before Makefile.
#
# In the future compiling lib/llvm will be incorporated into Makefile
# and GNUmakefile will be removed.
#
# The version of llvm can be controlled by passing llvm_proj on the command
# line. The default is llvm-3.9.1 if its not specified and maybe set to
# llvm_proj=llvm-6.0.0.

root_dir := $(shell pwd)
llvm_dir := $(root_dir)/lib/llvm
llvm_proj := llvm-3.9.1

pony_lib_llvm := $(llvm_dir)
llvm_config := $(pony_lib_llvm)/dist/bin/llvm-config

new_path := $(pony_lib_llvm)/dist/bin:$(PATH)

pony_targets := libponyc libponyrt libponyrt-pic libponyc.tests libponyrt.tests libponyc.benchmarks
pony_targets += libponyrt.benchmarks ponyc benchmark install uninstall stats test all
pony_targets += stdlib test-stdlib stdlib-debug test-stdlib-debug test-examples test-ci docs-online

.PHONY: $(pony_targets)
$(pony_targets): $(llvm_config)
	@PATH=$(new_path) $(MAKE) -f Makefile $(MAKECMDGOALS) $(MAKEFLAGS)

$(llvm_config):
	@$(MAKE) -C $(pony_lib_llvm) $(llvm_proj)
	@$(MAKE) -C $(pony_lib_llvm) install

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
	@echo '  llvm_proj=Proj       Make llvm where Proj is one of:'
	@echo '                         llvm-3.9.1 (default)'
	@echo '                         llvm-6.0.0'
	@echo
	@echo 'USE OPTIONS:'
	@echo '   valgrind'
	@echo '   pooltrack'
	@echo '   dtrace'
	@echo '   actor_continuations'
	@echo '   coverage'
	@echo '   scheduler_scaling_pthreads'
	@echo '   llvm_link_static'
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
