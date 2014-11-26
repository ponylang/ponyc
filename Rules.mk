empty :=
space := $(empty) $(empty)

##########################################################################
#                                                                        #
# DIRECTORY: determines the source dir of a specific target              #
#                                                                        #
# ENUMERATE: enumerates input and output files for a specific target     #
#                                                                        #
# PICK_COMPILER: Chooses a C or C++ compiler depending on the target.    #
#                                                                        #
# EXPAND_COMMAND: macro that expands to a proper make command for each   #
#                 target.                                                #
##########################################################################
define DIRECTORY
  $(eval sourcedir := )
  $(eval outdir := $(obj)/$(1))

  ifdef $(1).dir
    sourcedir := $($(1).dir)
  else ifneq ($$(filter $(1),$(tests)),)
    sourcedir := $(PONY_TEST_DIR)/$(subst .tests,,$(1))
    outdir := $(obj)/tests/$(subst .tests,,$(1))
  else
    sourcedir := $(PONY_SOURCE_DIR)/$(1)
  endif
endef

define ENUMERATE
  $(eval sourcefiles := )

  ifdef $(1).files
    sourcefiles := $$($(1).files)
  else
    sourcefiles := $$(shell find $$(sourcedir) -type f -name "*.c" -or -name "*.cc" | grep -v '.*/\.')
  endif

  ifdef $(1).except
    sourcefiles := $$(filter-out $($(1).except),$$(sourcefiles))
  endif
endef

define PICK_COMPILER
  $(eval compiler := $(CC) $(ALL_CFLAGS))

  ifneq ($$(filter $(suffix $(sourcefiles)),.cc),)
    compiler := $(CXX) $(ALL_CXXFLAGS)
  endif
endef

define PREPARE
  $(eval $(call DIRECTORY,$(1)))
  $(eval $(call ENUMERATE,$(1)))
  $(eval $(call PICK_COMPILER,$(sourcefiles)))
  $(eval objectfiles  := $(subst $(sourcedir)/,$(outdir)/,$(addsuffix .o,$(sourcefiles))))
  $(eval dependencies := $(subst .o,.d,$(objectfiles)))
endef

define EXPAND_OBJCMD
$(subst .c,,$(subst .cc,,$(1))): $(subst $(outdir)/,$(sourcedir)/,$(subst .o,,$(1)))
	@echo 'COMPILE $$(notdir $$<)'
	@mkdir -p $$(dir $$@)
	@$(compiler) $(BUILD_FLAGS) -c -o $$@ $$< -I $(subst $(space), -I ,$($(2).include))
endef

define EXPAND_DEPCMD
$(subst .c,,$(subst .cc,,$(1))): $(subst $(outdir)/,$(sourcedir)/,$(subst .d,,$(1)))
	@echo 'DEPEND $$(notdir $$@)'
	@mkdir -p $$(dir $$@)
	@$(compiler) -MMD -MP $$(subst .d,.o,$$@) -MF $$@ $(BUILD_FLAGS) $$<
endef

define EXPAND_COMMAND
$(eval $(call PREPARE,$(1)))
$(eval ofiles := $(subst .c,,$(subst .cc,,$(objectfiles))))

ifneq ($(filter $(1),$(libraries)),)
$(1): $(ofiles)
	@echo 'LINK $(1)'
	@$(AR) -rcs $($(1))/$(1).$(LIB_EXT) $(ofiles) $(LINKER_FLAGS) $($(1).ldflags) $($(1).links)
else
$(1): $(ofiles)
	@echo 'BINARY $(1)'
	@$(compiler) -o $($(1))/$(1) $(ofiles) $(LINKER_FLAGS) $($(1).ldflags) $($(1).links)
endif

$(foreach ofile,$(objectfiles),$(eval $(call EXPAND_OBJCMD,$(ofile),$(1))))
$(foreach dfile,$(dependencies),$(eval $(call EXPAND_DEPCMD,$(dfile))))

endef
