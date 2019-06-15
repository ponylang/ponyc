# Build llvm library
#  See README.md and `make help` for instructions

ifeq ($(OS),Windows_NT)
  OSTYPE = windows
else
  UNAME_S := $(shell uname -s)

  ifeq ($(UNAME_S),Linux)
    OSTYPE = linux

    ifndef AR
      ifneq (,$(shell which gcc-ar 2> /dev/null))
        AR = gcc-ar
      endif
    endif

    ALPINE=$(wildcard /etc/alpine-release)
  endif

  ifeq ($(UNAME_S),Darwin)
    OSTYPE = osx
  endif

  ifeq ($(UNAME_S),FreeBSD)
    OSTYPE = bsd
    CXX = c++
  endif

  ifeq ($(UNAME_S),DragonFly)
    OSTYPE = bsd
    CXX = c++
  endif

  ifeq ($(UNAME_S),OpenBSD)
    OSTYPE = bsd
    CXX = c++
    LLVM_CONFIG = /usr/local/bin/llvm-config
    default_pic = true
  endif
endif

# Set LLVM_CFG if not set
ifeq (,$(LLVM_CFG))
  LLVM_CFG := llvm-default.cfg
endif

# Set LLVM_RULES if not set
ifeq (,$(LLVM_RULES))
  LLVM_RULES := llvm-base.rules
endif

# Look for greadlink or readlink
ifeq ($(OSTYPE), osx)
  # On osx only use greadlink as readlink, if it exists,
  # doesn't support -e parameter.
  ifneq (,$(shell which greadlink 2> /dev/null))
    LLVM_READ_LINK = greadlink
  else
    $(error No greadlink command, for Mac OS X `install coreutils`!)
  endif
else ifneq (,$(shell which greadlink 2> /dev/null))
  LLVM_READ_LINK = greadlink
else ifneq (,$(shell which readlink 2> /dev/null))
  LLVM_READ_LINK = readlink
else
  $(error No greadlink or readlink command)
endif

LLVM_ROOT_DIR := $(shell pwd)

# Include configuration
ifneq (none,$(LLVM_CFG))
  include $(LLVM_CFG)
endif

# Some defaults
LLVM_BUILD_ENGINE ?= Unix Makefiles
LLVM_BUILD_TYPE ?= Release

# Define LLVM_LINKER to pass LLVM_USE_LINKER to have llvm output -fuse-ld=${LLVM_LINKER}
#LLVM_LINKER ?= gold

# There must always be a LLVM_CHECKOUT_REF
ifeq (,$(LLVM_CHECKOUT_REF))
  $(error No LLVM_CHECKOUT_REF such as: branch, tag or SHA1)
endif

# If no LLVM_TGT_TAG build one
LLVM_TGT_TAG ?= $(subst /,-,$(LLVM_CHECKOUT_REF))-$(LLVM_BUILD_TYPE)

LLVM_INSTALL_DIR_SYMLINK := $(LLVM_ROOT_DIR)/dist
LLVM_SRC_DIR := $(LLVM_ROOT_DIR)/src
LLVM_BASE_GEN_DIR := $(LLVM_ROOT_DIR)/gen
LLVM_GEN_DIR := $(LLVM_BASE_GEN_DIR)/$(LLVM_TGT_TAG)

LLVM_BUILD_DIR := $(LLVM_GEN_DIR)/build
LLVM_INSTALL_DIR := $(LLVM_GEN_DIR)/dist
LLVM_TGT_DIR := $(LLVM_GEN_DIR)/tgt

ifneq (,${LLVM_LINKER})
  LLVM_USE_LINKER := -DLLVM_USE_LINKER=$(LLVM_LINKER)
endif

ifneq (,$(verbose))
  VERBOSE_CMAKE := -DCMAKE_VERBOSE_MAKEFILE=true
endif

# Define the list of projects to build
ifeq ($(LLVM_PROJECT_LIST),all)
  PROJECT_LIST := all
else ifeq ($(LLVM_PROJECT_LIST),most)
  PROJECT_LIST = clang;clang-tools-extra;libcxx;libcxxabi;libunwind;compiler-rt;lld;lldb;polly;debuginfo-tests
else ifneq ($(LLVM_PROJECT_LIST),)
  # Use the passed list
  PROJECT_LIST := $(LLVM_PROJECT_LIST)
else
  # Use empty list which means only LLVM is created
  PROJECT_LIST :=
endif

ifneq ($(PROJECT_LIST),)
  LLVM_ENABLE_PROJECTS := -DLLVM_ENABLE_PROJECTS="$(PROJECT_LIST)"
else
  LLVM_ENABLE_PROJECTS :=
endif

# If no target is given the default is llvm-default and if
ifeq (,$(MAKECMDGOALS))
  MAKECMDGOALS := all
endif

ifeq ($(MAKECMDGOALS),install)
  GET_LLVM_SRC_TARGET := $(LLVM_TGT_DIR)/get-nothing
else ifeq ($(MAKECMDGOALS),distclean)
  # distclean needs nothing else initialized
else ifeq ($(MAKECMDGOALS),rebuild)
  GET_LLVM_SRC_TARGET := $(LLVM_TGT_DIR)/get-nothing
else ifeq ($(MAKECMDGOALS),get-submodule)
  # nothing to init
else
  # Check and see if we've installed this version. If so we don't need to get the source.
  READ_LINK_RSLT := $(shell $(LLVM_READ_LINK) -e $(LLVM_INSTALL_DIR_SYMLINK))
  ifeq ($(READ_LINK_RSLT), $(LLVM_INSTALL_DIR))
    GET_LLVM_SRC_TARGET := $(LLVM_TGT_DIR)/get-nothing
  else
    GET_LLVM_SRC_TARGET := $(LLVM_TGT_DIR)/get-llvm-src-$(LLVM_TGT_TAG)
  endif
endif

# Output some debug before running any recepies
#$(info lib/llvm/Makefile MAKECMDGOALS="$(MAKECMDGOALS)")
#$(info lib/llvm/Makefile LLVM_CFG="$(LLVM_CFG)")
#$(info lib/llvm/Makefile LLVM_RULES="$(LLVM_RULES)")
#$(info lib/llvm/Makefile LLVM_CHECKOUT_REF="$(LLVM_CHECKOUT_REF)")
#$(info lib/llvm/Makefile LLVM_READ_LINK="$(LLVM_READ_LINK)")
#$(info lib/llvm/Makefile LLVM_TGT_TAG="$(LLVM_TGT_TAG)")
#$(info lib/llvm/Makefile LLVM_TGT_DIR="$(LLVM_TAG_DIR)")
#$(info lib/llvm/Makefile LLVM_BUILD_ENGINE="$(LLVM_BUILD_ENGINE)")
#$(info lib/llvm/Makefile LLVM_BUILD_TYPE="$(LLVM_BUILD_TYPE)")
#$(info lib/llvm/Makefile LLVM_LINKER="$(LLVM_LINKER)")
#$(info lib/llvm/Makefile LLVM_USE_LINKER="$(LLVM_USE_LINKER)")
#$(info lib/llvm/Makefile LLVM_SRC_DIR="$(LLVM_SRC_DIR)")
#$(info lib/llvm/Makefile LLVM_BUILD_DIR="$(LLVM_BUILD_DIR)")
#$(info lib/llvm/Makefile LLVM_INSTALL_DIR="$(LLVM_INSTALL_DIR)")
#$(info lib/llvm/Makefile LLVM_INSTALL_DIR_SYMLINK="$(LLVM_INSTALL_DIR_SYMLINK)")
#$(info lib/llvm/Makefile READ_LINK_RSLT="$(READ_LINK_RSLT)")
#$(info lib/llvm/Makefile GET_LLVM_SRC_TARGET="$(GET_LLVM_SRC_TARGET)")
#$(info lib/llvm/Makefile LLVM_PROJECT_LIST="$(LLVM_PROJECT_LIST)")
#$(info lib/llvm/Makefile PROJECT_LIST="$(PROJECT_LIST)")

# Include the rules
#  Having LLVM_RULES a separate file allows it to be specified
#  in the xxx.cfg file and thus define its define its own rules.
include $(LLVM_RULES)
