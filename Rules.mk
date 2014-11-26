##########################################################################
#                                                                        #
# DIRECTORY: Determines the source dir of a specific target              #
#                                                                        #
# ENUMERATE: Enumerates input and output files for a specific target     #
#                                                                        #
# PICK_COMPILER: Chooses a C or C++ compiler depending on the target.    #
#                                                                        #
# CONFIGURE_LIBS: Builds a string of libraries to link for a targets     #
#                 link dependency.                                       #
#                                                                        #
# CONFIGURE_LINKER: Assembles the linker flags required for a target.    #
#                                                                        #
# EXPAND_COMMAND: Macro that expands to a proper make command for each   #
#                 target.                                                #
#                                                                        #
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
  $(eval compiler := $(CC))
	$(eval flags:= $(ALL_CFLAGS))

  ifneq ($$(filter $(suffix $(sourcefiles)),.cc),)
    compiler := $(CXX)
		flags := $(ALL_CXXFLAGS)
  endif
endef

define CONFIGURE_LIBS
  ifneq (,$$(filter $(1),$(prebuilt)))
    linkcmd += $($(1).ldflags)
    libs += $($(1).libs)
  else
    libs += -l$(subst lib,,$(1))
  endif
endef

define CONFIGURE_LINKER
  $(eval linker := $(compiler))
  $(eval linkcmd := $(LINKER_FLAGS) -L $(lib))
  $(eval libs :=)
  $(foreach lk,$($(1).links),$(eval $(call CONFIGURE_LIBS,$(lk))))
  linkcmd += $(libs)

  ifdef $(1).linker
    linker := $($(1).linker)
  endif
endef

define PREPARE
  $(eval $(call DIRECTORY,$(1)))
  $(eval $(call ENUMERATE,$(1)))
  $(eval $(call PICK_COMPILER,$(sourcefiles)))
  $(eval $(call CONFIGURE_LINKER,$(1)))
  $(eval objectfiles  := $(subst $(sourcedir)/,$(outdir)/,$(addsuffix .o,$(sourcefiles))))
  $(eval dependencies := $(subst .c,,$(subst .cc,,$(subst .o,.d,$(objectfiles)))))
endef

define EXPAND_OBJCMD
$(subst .c,,$(subst .cc,,$(1))): $(subst $(outdir)/,$(sourcedir)/,$(subst .o,,$(1)))
	@echo '$$(notdir $$<)'
	@mkdir -p $$(dir $$@)
	@$(compiler) -MMD -MP $(BUILD_FLAGS) $(flags) -c -o $$@ $$< $($(2).options) $(addprefix -I,$($(2).include))
endef

define EXPAND_COMMAND
$(eval $(call PREPARE,$(1)))
$(eval ofiles := $(subst .c,,$(subst .cc,,$(objectfiles))))

print_$(1):
	@echo "==== Building $(1) ($(config)) ===="

ifneq ($(filter $(1),$(libraries)),)
$(1): print_$(1) $($(1))/$(1).$(LIB_EXT)
$($(1))/$(1).$(LIB_EXT): $(ofiles)
	@echo 'Linking $(1)'
	@$(AR) -rcs $$@ $(ofiles)
else
$(1): print_$(1) $($(1))/$(1)
$($(1))/$(1): print_$(1) $(ofiles)
	@echo 'Linking $(1)'
	@$(linker) -o $$@ $(ofiles) $(linkcmd)
endif

$(foreach ofile,$(objectfiles),$(eval $(call EXPAND_OBJCMD,$(ofile),$(1))))
-include $(dependencies)
endef
