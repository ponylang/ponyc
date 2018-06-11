# Build llvm library
#
# Based on [yurydelendik wasmllvm](https://gist.github.com/yurydelendik/4eeff8248aeb14ce763e)

ROOT_DIR := $(shell pwd)
LLVM_URL := https://github.com/ponylang/llvm.git

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

LLVM_SRC_DIR := $(ROOT_DIR)/src
LLVM_BUILD_DIR := $(ROOT_DIR)/build

LLVM_BUILD_ENGINE := "Unix Makefiles"
LLVM_BUILD_TYPE := Release
LLVM_INSTALL_DIR := $(ROOT_DIR)/dist

LLVM_LINKER := bfd
ifeq (llvm-3.9.1,$(LLVM_PROJ))
  # 3.9.1 doesn't supprot -DLLVM_USE_LINKER so make it empty to supress a warning
  LLVM_USE_LINKER=
else
  LLVM_USE_LINKER=-DLLVM_USE_LINKER=$(LLVM_LINKER)
endif

ifeq ($(LLVM_BUILD_ENGINE),Ninja)
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

.PHONY: clean-except-get
clean-except-get:
	rm -rf $(LLVM_BUILD_DIR) $(LLVM_INSTALL_DIR)
	rm -f built-* installed-*

.PHONY: clean
clean: clean-except-get

.PHONY: distclean
distclean: clean-except-get
	rm -rf $(LLVM_SRC_DIR)
	rm -f get-*

.PHONY: install
install: installed-$(LLVM_PROJ)

installed-$(LLVM_PROJ):
	@echo install $(LLVM_PROJ)
	@rm -rf $(LLVM_INSTALL_DIR)
	@mkdir -p $(LLVM_INSTALL_DIR)
	$(MAKE) -C $(LLVM_BUILD_DIR) install
	touch installed-$(LLVM_PROJ)

built-llvm-$(LLVM_PROJ): $(LLVM_BUILD_DIR)/generated-llvm-makefile-$(LLVM_PROJ)
	@echo building $(LLVM_PROJ)
	$(MAKE) -C $(LLVM_BUILD_DIR)
	touch built-llvm-$(LLVM_PROJ)

$(LLVM_BUILD_DIR)/generated-llvm-makefile-$(LLVM_PROJ): $(GET_LLVM_SRC_TARGET)
	@echo generate $(LLVM_PROJ) makefile
	mkdir -p $(LLVM_BUILD_DIR)
	cd $(LLVM_BUILD_DIR); cmake -G $(LLVM_BUILD_ENGINE) $(VERBOSE_CMAKE) $(LLVM_USE_LINKER) -DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR) -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly -DCMAKE_BUILD_TYPE=$(LLVM_BUILD_TYPE) $(LLVM_SRC_DIR)
	touch $(LLVM_BUILD_DIR)/generated-llvm-makefile-$(LLVM_PROJ)

get-nothing: clean

get-default-llvm-src:
	@echo get-default-llvm-src
	$(MAKE) clean
	rm -rf $(LLVM_SRC_DIR)
	mkdir $(LLVM_SRC_DIR)
	git submodule init
	git submodule update $(LLVM_SRC_DEPTH)
	touch get-default-llvm-src

get-llvm-src-$(LLVM_PROJ):
	@echo get-llvm-src-$(LLVM_PROJ)
	$(MAKE) clean
	rm -rf $(LLVM_SRC_DIR)
	git clone $(LLVM_BRANCH) $(LLVM_SRC_DEPTH) $(LLVM_URL) $(LLVM_SRC_DIR)
	touch get-llvm-src-$(LLVM_PROJ)
