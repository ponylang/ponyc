# Build llvm library
#
# Based on [yurydelendik wasmllvm](https://gist.github.com/yurydelendik/4eeff8248aeb14ce763e)

ROOT_DIR := $(shell pwd)
LLVM_URL := https://github.com/winksaville/llvm.git

$(warning MAKECMDGOALS=$(MAKECMDGOALS) llvm_proj=$(llvm_proj))

ifeq ($(MAKECMDGOALS),llvm-6.0.0)
  LLVM_PROJ := llvm-6.0.0
  GET_LLVM_SRC_TARGET := get-llvm-src-$(LLVM_PROJ)
  LLVM_SRC_DEPTH := --depth=1
  LLVM_BRANCH := -b release_60
else ifeq ($(MAKECMDGOALS),llvm-3.9.1)
  LLVM_PROJ := llvm-3.9.1
  GET_LLVM_SRC_TARGET := get-llvm-src-$(LLVM_PROJ)
  LLVM_SRC_DEPTH := --depth=1
  LLVM_BRANCH := -b release_39
else ifeq ($(MAKECMDGOALS),llvm-current)
  LLVM_PROJ := llvm-current
  GET_LLVM_SRC_TARGET := get-nothing
  LLVM_SRC_DEPTH :=
  LLVM_BRANCH :=
else
  LLVM_PROJ := llvm-default
  GET_LLVM_SRC_TARGET := get-default-llvm-src
  LLVM_SRC_DEPTH := --depth=1
  LLVM_URL :=
  LLVM_BRANCH :=
endif

LLVM_SRCDIR := $(ROOT_DIR)/src
LLVM_BUILDDIR := $(ROOT_DIR)/build

BUILD_ENGINE := "Unix Makefiles"
BUILD_TYPE := Release
LLVM_INSTALLDIR := $(ROOT_DIR)/dist

LINKER := bfd
ifeq (llvm-3.9.1,$(LLVM_PROJ))
  # 3.9.1 doesn't supprot -DLLVM_USE_LINKER so make it empty to supress a warning
  USE_LINKER=
else
  USE_LINKER=-DLLVM_USE_LINKER=$(LINKER)
endif

ifeq ($(BUILD_ENGINE),Ninja)
MAKE := ninja
MAKEFILE := build.ninja
else
MAKE := make
MAKEFILE := Makefile
endif

ifneq (,$(verbose))
  VERBOSE_CMAKE := -DCMAKE_VERBOSE_MAKEFILE=true
endif

$(LLVM_PROJ): built-llvm-$(LLVM_PROJ)

.PHONY: clean
clean:
	rm -rf $(LLVM_BUILDDIR) $(LLVM_INSTALLDIR)
	rm -f built-* installed-* get-*

.PHONY: install
install: installed-$(LLVM_PROJ)

installed-$(LLVM_PROJ):
	@echo install $(LLVM_PROJ)
	@rm -rf $(LLVM_INSTALLDIR)
	@mkdir -p $(LLVM_INSTALLDIR)
	$(MAKE) -C $(LLVM_BUILDDIR) install
	touch installed-$(LLVM_PROJ)

built-llvm-$(LLVM_PROJ): $(LLVM_BUILDDIR)/generated-llvm-makefile-$(LLVM_PROJ)
	@echo building $(LLVM_PROJ)
	$(MAKE) -C $(LLVM_BUILDDIR)
	touch built-llvm-$(LLVM_PROJ)

$(LLVM_BUILDDIR)/generated-llvm-makefile-$(LLVM_PROJ): $(GET_LLVM_SRC_TARGET)
	@echo generate $(LLVM_PROJ) makefile
	mkdir -p $(LLVM_BUILDDIR)
	cd $(LLVM_BUILDDIR); cmake -G $(BUILD_ENGINE) $(VERBOSE_CMAKE) $(USE_LINKER) -DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALLDIR) -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(LLVM_SRCDIR)
	touch $(LLVM_BUILDDIR)/generated-llvm-makefile-$(LLVM_PROJ)

get-nothing:
	@echo get-nothing
	$(MAKE) clean
	touch get-nothing

get-default-llvm-src:
	@echo get-default-llvm-src
	$(MAKE) clean
	rm -rf $(LLVM_SRCDIR)
	mkdir $(LLVM_SRCDIR)
	git submodule init
	git submodule update $(LLVM_SRC_DEPTH)
	touch get-default-llvm-src

get-llvm-src-$(LLVM_PROJ):
	@echo get-llvm-src-$(LLVM_PROJ)
	$(MAKE) clean
	rm -rf $(LLVM_SRCDIR)
	git clone $(LLVM_BRANCH) $(LLVM_SRC_DEPTH) $(LLVM_URL) $(LLVM_SRCDIR)
	touch get-llvm-src-$(LLVM_PROJ)
