# Build llvm library and optionally clang and compiler-rt
# Based on [yurydelendik wasmllvm](https://gist.github.com/yurydelendik/4eeff8248aeb14ce763e)

ROOT_DIR=$(shell pwd)

$(warning MAKECMDGOALS=$(MAKECMDGOALS) LLVM_PROJ=$(LLVM_PROJ))

ifeq ($(MAKECMDGOALS),llvm-6.0.0)
  LLVM_PROJ=llvm-6.0.0
else ifeq ($(MAKECMDGOALS),llvm-3.9.1)
  LLVM_PROJ=llvm-3.9.1
else
  LLVM_PROJ=llvm-3.9.1
  MAKECMDGOALS += llvm-3.9.1
endif

LLVM_SRCDIR=$(ROOT_DIR)/src-$(LLVM_PROJ)
LLVM_BUILDDIR=$(ROOT_DIR)/build

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

$(LLVM_PROJ): built-llvm-$(LLVM_PROJ)

.PHONY: clean
clean:
	rm -rf $(LLVM_BUILDDIR) $(LLVM_INSTALLDIR)
	rm -f built-* installed-*

.PHONY: install
install: installed-$(LLVM_PROJ)

installed-$(LLVM_PROJ):
	@echo install $(LLVM_PROJ)
	@rm -rf $(LLVM_INSTALLDIR)
	@mkdir -p $(LLVM_INSTALLDIR)
	$(MAKE) -C $(LLVM_BUILDDIR) install
	@touch installed-$(LLVM_PROJ)

built-llvm-$(LLVM_PROJ): $(LLVM_BUILDDIR)/generated-llvm-makefile-$(LLVM_PROJ)
	@echo building $(LLVM_PROJ)
	$(MAKE) -C $(LLVM_BUILDDIR)
	@touch built-llvm-$(LLVM_PROJ)

$(LLVM_BUILDDIR)/generated-llvm-makefile-$(LLVM_PROJ):
	@echo generate $(LLVM_PROJ) makefile
	$(MAKE) clean
	mkdir -p $(LLVM_BUILDDIR)
	cd $(LLVM_BUILDDIR); cmake -G $(BUILD_ENGINE) -DLLVM_USE_LINKER=$(LINKER) -DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALLDIR) -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(LLVM_SRCDIR)
	@touch $(LLVM_BUILDDIR)/generated-llvm-makefile-$(LLVM_PROJ)
