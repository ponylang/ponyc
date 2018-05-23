# Build llvm library and optionally clang and compiler-rt
# Based on [yurydelendik wasmllvm](https://gist.github.com/yurydelendik/4eeff8248aeb14ce763e)

ROOT_DIR=$(shell pwd)
LLVM_SRCDIR=$(ROOT_DIR)/src
LLVM_BUILDDIR=$(ROOT_DIR)/build
#LLVM_PROJ=tags/RELEASE_600/final
LLVM_PROJ=tags/RELEASE_391/final
LLVM_PROJ_LABEL=$(subst /,-,$(LLVM_PROJ))

BUILD_ENGINE="Unix Makefiles"
BUILD_TYPE=Release
LLVM_INSTALLDIR=$(ROOT_DIR)/dist
LINKER=bfd

ifeq ($(BUILD_ENGINE),Ninja)
MAKE=ninja
MAKEFILE=build.ninja
else
MAKE=make
MAKEFILE=Makefile
endif

# This error, https://github.com/google/sanitizers/issues/822,
# occurred when attempting to compile llvm and clang 3.9.1:
#   src/llvm/projects/compiler-rt/lib/sanitizer_common/sanitizer_stoptheworld_linux_libcdep.cc: In function ‘int __sanitizer::TracerThread(void*)’:
#   src/llvm/projects/compiler-rt/lib/sanitizer_common/sanitizer_stoptheworld_linux_libcdep.cc:276:22: error: aggregate ‘sigaltstack handler_stack’ has incomplete type and cannot be defined
#      struct sigaltstack handler_stack;
# So disallowing compiling of clang for llvm 3.9.1
ifeq ($(MAKECMDGOALS),all)
  ifneq (,$(findstring 391,$(LLVM_PROJ)))
    $(error Cannot compile clang with llvm 3.9.1)
  endif
endif

#.PHONY: xyz
#xyz:
#	@echo xyz

.PHONY: llvm
llvm: built-llvm-$(LLVM_PROJ_LABEL)

.PHONY: all
all: built-clang-$(LLVM_PROJ_LABEL)

.PHONY: clean
clean:
	rm -rf $(LLVM_BUILDDIR) $(LLVM_INSTALLDIR)
	rm -f built-* installed-*

.PHONY: distclean
distclean: clean
	rm -rf $(LLVM_SRCDIR)

.PHONY: install
install: installed-$(LLVM_PROJ_LABEL)

installed-$(LLVM_PROJ_LABEL):
	@echo install
	@mkdir -p $(LLVM_INSTALLDIR)
	$(MAKE) -C $(LLVM_BUILDDIR) install
	@touch installed-$(LLVM_PROJ_LABEL)

built-llvm-$(LLVM_PROJ_LABEL): $(LLVM_BUILDDIR)/generated-llvm-makefile-$(LLVM_PROJ_LABEL)
	@echo building llvm
	$(MAKE) -C $(LLVM_BUILDDIR)
	@touch built-llvm-$(LLVM_PROJ_LABEL)

built-clang-$(LLVM_PROJ_LABEL): $(LLVM_BUILDDIR)/generated-clang-makefile-$(LLVM_PROJ_LABEL)
	@echo building clang
	$(MAKE) -C $(LLVM_BUILDDIR)
	@touch built-clang-$(LLVM_PROJ_LABEL)

$(LLVM_BUILDDIR)/made-builddir-$(LLVM_PROJ_LABEL):
	@echo making $(LLVM_BUILDDIR)
	rm -rf $(LLVM_BUILDDIR)
	rm -f built-* installed-*
	mkdir -p $(LLVM_BUILDDIR)
	@touch $(LLVM_BUILDDIR)/made-builddir-$(LLVM_PROJ_LABEL)

$(LLVM_SRCDIR)/made-srcdir-$(LLVM_PROJ_LABEL):
	@echo making $(LLVM_SRCDIR)
	rm -rf $(LLVM_SRCDIR)
	rm -f built-* installed-*
	mkdir -p $(LLVM_SRCDIR)
	@touch $(LLVM_SRCDIR)/made-srcdir-$(LLVM_PROJ_LABEL)

$(LLVM_SRCDIR)/got-llvm-$(LLVM_PROJ_LABEL): $(LLVM_SRCDIR)/made-srcdir-$(LLVM_PROJ_LABEL) $(LLVM_BUILDDIR)/made-builddir-$(LLVM_PROJ_LABEL)
	@echo get llvm
	svn co http://llvm.org/svn/llvm-project/llvm/$(LLVM_PROJ) $(LLVM_SRCDIR)
	@touch $(LLVM_SRCDIR)/got-llvm-$(LLVM_PROJ_LABEL)

$(LLVM_BUILDDIR)/generated-llvm-makefile-$(LLVM_PROJ_LABEL): $(LLVM_SRCDIR)/got-llvm-$(LLVM_PROJ_LABEL)
	@echo generate llvm makefile
	cd $(LLVM_BUILDDIR); cmake -G $(BUILD_ENGINE) -DLLVM_USE_LINKER=$(LINKER) -DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALLDIR) -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(LLVM_SRCDIR)
	@touch $(LLVM_BUILDDIR)/generated-llvm-makefile-$(LLVM_PROJ_LABEL)

$(LLVM_SRCDIR)/tools/clang/got-clang-$(LLVM_PROJ_LABEL): $(LLVM_SRCDIR)/got-llvm-$(LLVM_PROJ_LABEL)
	@echo get clang
	svn co http://llvm.org/svn/llvm-project/cfe/$(LLVM_PROJ) $(LLVM_SRCDIR)/tools/clang
	svn co http://llvm.org/svn/llvm-project/compiler-rt/$(LLVM_PROJ) $(LLVM_SRCDIR)/projects/compiler-rt
	@touch $(LLVM_SRCDIR)/tools/clang/got-clang-$(LLVM_PROJ_LABEL)

$(LLVM_BUILDDIR)/generated-clang-makefile-$(LLVM_PROJ_LABEL): $(LLVM_SRCDIR)/tools/clang/got-clang-$(LLVM_PROJ_LABEL)
	@echo generate clang makefile
	cd $(LLVM_BUILDDIR); cmake -G $(BUILD_ENGINE) -DLLVM_USE_LINKER=$(LINKER) -DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALLDIR) -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(LLVM_SRCDIR)
	@touch $(LLVM_BUILDDIR)/generated-clang-makefile-$(LLVM_PROJ_LABEL)
