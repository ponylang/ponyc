ROOT_DIR=$(shell pwd)
LLVM_WORKDIR=$(ROOT_DIR)/src/llvm
LLVM_BUILDDIR=$(ROOT_DIR)/build/llvm
#LLVM_PROJ=trunk
LLVM_PROJ=tags/RELEASE_600/final

#BUILD_ENGINE="Unix Makefiles"
BUILD_ENGINE=Ninja
BUILD_TYPE=Release
CPUS=1
INSTALL_DIR=$(ROOT_DIR)/dist
LINKER=gold


ifeq ($(BUILD_ENGINE),Ninja)
MAKE=ninja
MAKEFILE=build.ninja
else
MAKE=make
MAKEFILE=Makefile
endif

build: build-llvm

update: update-llvm

clean: clean-llvm
	rm -rf $(INSTALL_DIR)

dist-clean: dist-clean-llvm
	rm -rf build/ dist/ src/

install: install-llvm

$(LLVM_WORKDIR)/.svn:
	rm -rf $(LLVM_WORKDIR)
	mkdir -p $(LLVM_WORKDIR)
	svn co http://llvm.org/svn/llvm-project/llvm/$(LLVM_PROJ) $(LLVM_WORKDIR)
	svn co http://llvm.org/svn/llvm-project/cfe/$(LLVM_PROJ) $(LLVM_WORKDIR)/tools/clang
	svn co http://llvm.org/svn/llvm-project/compiler-rt/$(LLVM_PROJ) $(LLVM_WORKDIR)/projects/compiler-rt

update-llvm:
	mkdir -p $(LLVM_WORKDIR)
	svn update $(LLVM_WORKDIR)
	svn update $(LLVM_WORKDIR)/tools/clang
	svn update $(LLVM_WORKDIR)/projects/compiler-rt

$(LLVM_BUILDDIR)/$(MAKEFILE): $(LLVM_WORKDIR)/.svn
	mkdir -p $(LLVM_BUILDDIR)
	cd $(LLVM_BUILDDIR); cmake -G $(BUILD_ENGINE) -DLLVM_USE_LINKER=$(LINKER) -DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(LLVM_WORKDIR)

build-llvm: $(LLVM_BUILDDIR)/$(MAKEFILE)
	$(MAKE) -C $(LLVM_BUILDDIR) -j $(CPUS)

clean-llvm:
	rm -rf $(LLVM_BUILDDIR)

dist-clean-llvm: clean-llvm
	rm -rf $(LLVM_WORKDIR)

install-llvm:
	mkdir -p $(INSTALL_DIR)
	$(MAKE) -C $(LLVM_BUILDDIR) install

#install-llvm-headers:
#	$(MAKE) -C $(LLVM_BUILDDIR) install-llvm-headers
#
#install-clang-headers:
#	$(MAKE) -C $(LLVM_BUILDDIR) install-clang-headers


.PHONY: build clean dist-clean install
.PHONY: build-llvm dist-clean-llvm clean-llvm install-llvm
