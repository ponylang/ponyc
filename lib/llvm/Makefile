# Build llvm library
#
# If target (aka. MAKECMDGOALS) is empty then llvm-default
# is assumed.
#
# A valid target is:
#   clean
#   distclean
#   llvm-6.0.0
#   llvm-3.9.1
#   llvm-default
#   llvm-current
#
# clean: Remove build/ and dist/
# distclean: remove build/, dist/ and src/
# llvm-6.0.0: Get, Build, Install llvm-6.0.0
# llvm-3.9.1: Get, Build, Install llvm-3.9.1
# llvm-default: Get, Build, Install the default which is llvm-3.9.1
# llvm-current: Build, Install what ever is in src/
#
# In the above Get, Build and Install are conditional
# based on a file created if the operation was previously
# successful.
#
# Based on [yurydelendik wasmllvm](https://gist.github.com/yurydelendik/4eeff8248aeb14ce763e)

ROOT_DIR := $(shell pwd)
LLVM_URL := https://github.com/ponylang/llvm.git

ifeq (,$(MAKECMDGOALS))
  MAKECMDGOALS := llvm-default
endif

#$(warning MAKECMDGOALS=$(MAKECMDGOALS))

ifeq ($(MAKECMDGOALS),rebuild)
  LLVM_PROJ := llvm-current
  GET_LLVM_SRC_TARGET := get-nothing
  LLVM_SRC_DEPTH :=
  LLVM_BRANCH :=
else ifeq ($(MAKECMDGOALS),clean)
  # Nothing to init
else ifeq ($(MAKECMDGOALS),distclean)
  # Nothing to init
else ifeq ($(MAKECMDGOALS),llvm-6.0.0)
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
else ifeq ($(MAKECMDGOALS),llvm-default)
  LLVM_PROJ := llvm-default
  GET_LLVM_SRC_TARGET := get-default-llvm-src
  LLVM_SRC_DEPTH := --depth=1
  LLVM_URL :=
  LLVM_BRANCH :=
else
  $(error Uknown target '$(MAKECMDGOALS)', someone did not pass a goal)
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

.PHONY: rebuild
rebuild: clean-built-installed
	@echo building $(LLVM_PROJ) `git -C src log -1 --pretty="format:hash=%h ref=%d subject=%s"`
	$(MAKE) -C $(LLVM_BUILD_DIR)
	touch built-llvm-$(LLVM_PROJ)
	@echo installing $(LLVM_PROJ)
	$(MAKE) -C $(LLVM_BUILD_DIR) install
	touch installed-llvm-$(LLVM_PROJ)

.PHONY: clean
clean: clean-except-get

.PHONY: distclean
distclean: clean-except-get
	rm -rf $(LLVM_SRC_DIR)
	rm -f get-*

.PHONY: clean-except-get
clean-except-get: clean-built-installed
	rm -rf $(LLVM_BUILD_DIR) $(LLVM_INSTALL_DIR)

.PHONY: clean-built-installed
clean-built-installed:
	rm -f built-* installed-*

built-llvm-$(LLVM_PROJ): $(LLVM_BUILD_DIR)/generated-llvm-makefile-$(LLVM_PROJ)
	$(MAKE) rebuild

$(LLVM_BUILD_DIR)/generated-llvm-makefile-$(LLVM_PROJ): $(GET_LLVM_SRC_TARGET)
	@echo generate $(LLVM_PROJ) `git -C src log -1 --pretty="format:hash=%h ref=%d subject=%s"`
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
