# Build llvm library
#
# If target (aka. MAKECMDGOALS) is empty then llvm-default
# is assumed.
#
# A valid target is:
#   clean
#   distclean
#   llvm-7.x
#   llvm-6.x
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
#

ROOT_DIR := $(shell pwd)
LLVM_URL := https://github.com/llvm/llvm-project.git

ifeq (,$(MAKECMDGOALS))
  MAKECMDGOALS := llvm-default
endif

$(info lib/llvm/Makefile MAKECMDGOALS="$(MAKECMDGOALS)")

ifeq ($(MAKECMDGOALS),rebuild)
  LLVM_TARGET := llvm-current
  GET_LLVM_SRC_TARGET := get-nothing
  LLVM_SRC_DEPTH :=
  LLVM_BRANCH :=
else ifeq ($(MAKECMDGOALS),llvm-7.x)
  LLVM_TARGET := llvm-7.x
  GET_LLVM_SRC_TARGET := get-llvm-src-$(LLVM_TARGET)
  LLVM_SRC_DEPTH :=
  LLVM_BRANCH := -b release/7.x
else ifeq ($(MAKECMDGOALS),llvm-6.x)
  LLVM_TARGET := llvm-6.x
  GET_LLVM_SRC_TARGET := get-llvm-src-$(LLVM_TARGET)
  LLVM_SRC_DEPTH :=
  LLVM_BRANCH := -b release/6.x
else ifeq ($(MAKECMDGOALS),llvm-3.9.1)
  LLVM_TARGET := llvm-3.9.1
  GET_LLVM_SRC_TARGET := get-llvm-src-$(LLVM_TARGET)
  LLVM_SRC_DEPTH :=
  LLVM_BRANCH := -b release/3.9.x
else ifeq ($(MAKECMDGOALS),llvm-current)
  LLVM_TARGET := llvm-current
  GET_LLVM_SRC_TARGET := get-nothing
  LLVM_SRC_DEPTH :=
  LLVM_BRANCH :=
else ifeq ($(MAKECMDGOALS),llvm-default)
  $(info llvm-default.....)
  LLVM_TARGET := llvm-default
  GET_LLVM_SRC_TARGET := get-default-llvm-src
  LLVM_SRC_DEPTH :=
  LLVM_URL :=
  LLVM_BRANCH :=
else ifeq ($(MAKECMDGOALS),buildit)
  # Nothing to init
else ifeq ($(MAKECMDGOALS),installit)
  # Nothing to init
else ifeq ($(MAKECMDGOALS),generateit)
  # Nothing to init
else ifeq ($(MAKECMDGOALS),clean)
  # Nothing to init
else ifeq ($(MAKECMDGOALS),distclean)
  # Nothing to init
#else
#  $(error Uknown target '$(MAKECMDGOALS)', someone did not pass a goal)
endif

$(info lib/llvm/Makefile LLVM_TARGET="$(LLVM_TARGET)")
$(info lib/llvm/Makefile LLVM_SRC_DIR="$(LLVM_SRC_DIR)")
$(info lib/llvm/Makefile LLVM_BUILD_DIR="$(LLVM_BUILD_DIR)")

LLVM_SRC_DIR := $(ROOT_DIR)/src
LLVM_BUILD_DIR := $(ROOT_DIR)/build
LLVM_INSTALL_DIR := $(ROOT_DIR)/dist

LLVM_BUILD_ENGINE := "Unix Makefiles"
LLVM_BUILD_TYPE := Release

# LLVM_lINKER valid options are bfd, gold or lld.
# Currently lld is only valid when compiler is clang{++}.
LLVM_LINKER := gold
LLVM_USE_LINKER := -DLLVM_USE_LINKER=$(LLVM_LINKER)

$(info lib/llvm/Makefile LLVM_PROJECT_LIST="$(LLVM_PROJECT_LIST)")
# LLVM_PROJECT_LIST is a semi-colon seperated list of projects defined
# by llvm.org. A value of "all", no quotes, will create all projects
# but I've found that using "all" won't always compile. The following
# PROJECT_LIST worked for 7.0.1:
# PROJECT_LIST = clang;clang-tools-extra;libcxx;libcxxabi;libunwind;compiler-rt;lld;lldb;polly;debuginfo-tests
ifneq ($(LLVM_PROJECT_LIST),)
  PROJECT_LIST = $(LLVM_PROJECT_LIST)
else
  # With PROJECT_LIST empty only LLVM will be built.
  PROJECT_LIST =
endif
$(info lib/llvm/Makefile PROJECT_LIST="$(PROJECT_LIST)")

ifneq ($(PROJECT_LIST),)
  LLVM_ENABLE_PROJECTS := -DLLVM_ENABLE_PROJECTS="$(PROJECT_LIST)"
else
  LLVM_ENABLE_PROJECTS :=
endif

ifneq (,$(verbose))
  VERBOSE_CMAKE := -DCMAKE_VERBOSE_MAKEFILE=true
endif

$(LLVM_TARGET): installed-llvm-$(LLVM_TARGET)

.PHONY: rebuild
rebuild: clean-built-installed
	$(MAKE) installed-llvm-$(LLVM_TARGET) LLVM_TARGET=$(LLVM_TARGET) LLVM_SRC_DIR=$(LLVM_SRC_DIR) LLVM_BUILD_DIR=$(LLVM_BUILD_DIR)

ifeq ($(LLVM_BUILD_ENGINE),Ninja)
.PHONY: buildit
buildit:
	@echo building $(LLVM_TARGET) `git -C src log -1 --pretty="format:hash=%h ref=%d subject='%s'"`
	ninja -C $(LLVM_BUILD_DIR)
	touch built-llvm-$(LLVM_TARGET)
else
.PHONY: buildit
buildit:
	@echo building $(LLVM_TARGET) `git -C src log -1 --pretty="format:hash=%h ref=%d subject='%s'"`
	$(MAKE) -C $(LLVM_BUILD_DIR)
	touch built-llvm-$(LLVM_TARGET)
endif

ifeq ($(LLVM_BUILD_ENGINE),Ninja)
.PHONY: installit
installit:
	@echo installing $(LLVM_TARGET)
	ninja -C $(LLVM_BUILD_DIR) install
	touch installed-llvm-$(LLVM_TARGET)
else
.PHONY: installit
installit:
	@echo installing $(LLVM_TARGET)
	$(MAKE) -C $(LLVM_BUILD_DIR) install
	touch installed-llvm-$(LLVM_TARGET)
endif

.PHONY: generateit
generateit:
	@echo generate $(LLVM_TARGET) `git -C src log -1 --pretty="format:hash=%h ref=%d"`
	mkdir -p $(LLVM_BUILD_DIR)
	cd $(LLVM_BUILD_DIR); cmake -G $(LLVM_BUILD_ENGINE) $(VERBOSE_CMAKE) $(LLVM_USE_LINKER) $(LLVM_ENABLE_PROJECTS) -DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR) -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly -DCMAKE_BUILD_TYPE=$(LLVM_BUILD_TYPE) -DLLVM_BUILD_LLVM_DYLIB=ON -DLLVM_LINK_LLVM_DYLIB=ON $(LLVM_SRC_DIR)/llvm
	touch $(LLVM_BUILD_DIR)/generated-llvm-makefile-$(LLVM_TARGET)

$(LLVM_BUILD_DIR)/generated-llvm-makefile-$(LLVM_TARGET): $(GET_LLVM_SRC_TARGET)
	$(MAKE) generateit LLVM_TARGET=$(LLVM_TARGET) LLVM_SRC_DIR=$(LLVM_SRC_DIR) LLVM_BUILD_DIR=$(LLVM_BUILD_DIR)

built-llvm-$(LLVM_TARGET): $(LLVM_BUILD_DIR)/generated-llvm-makefile-$(LLVM_TARGET)
	$(MAKE) buildit LLVM_TARGET=$(LLVM_TARGET) LLVM_SRC_DIR=$(LLVM_SRC_DIR) LLVM_BUILD_DIR=$(LLVM_BUILD_DIR)

installed-llvm-$(LLVM_TARGET): built-llvm-$(LLVM_TARGET)
	$(MAKE) installit LLVM_TARGET=$(LLVM_TARGET) LLVM_SRC_DIR=$(LLVM_SRC_DIR) LLVM_BUILD_DIR=$(LLVM_BUILD_DIR)

get-nothing:
	touch get-nothing

get-default-llvm-src:
	@echo get-default-llvm-src
	rm -rf $(LLVM_SRC_DIR)
	mkdir $(LLVM_SRC_DIR)
	git submodule sync
	git submodule update --init $(LLVM_SRC_DEPTH)
	touch get-default-llvm-src

get-llvm-src-$(LLVM_TARGET):
	@echo get-llvm-src-$(LLVM_TARGET)
	rm -rf $(LLVM_SRC_DIR)
	mkdir $(LLVM_SRC_DIR)
	git clone $(LLVM_BRANCH) $(LLVM_SRC_DEPTH) $(LLVM_URL) $(LLVM_SRC_DIR)
	touch get-llvm-src-$(LLVM_TARGET)

.PHONY: clean
clean: clean-except-get

.PHONY: distclean
distclean: clean-except-get
	rm -rf $(LLVM_SRC_DIR)
	mkdir $(LLVM_SRC_DIR)
	rm -f get-*

.PHONY: clean-except-get
clean-except-get: clean-built-installed
	rm -rf $(LLVM_BUILD_DIR) $(LLVM_INSTALL_DIR)

.PHONY: clean-built-installed
clean-built-installed:
	rm -f built-* installed-*
