config ?= release

PONYC ?= ponyc
PACKAGE := ast
GET_DEPENDENCIES_WITH := corral fetch
CLEAN_DEPENDENCIES_WITH := corral clean
COMPILE_WITH := corral run -- $(PONYC)


$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(COVERAGE_DIR):
	mkdir -p $(COVERAGE_DIR)

BUILD_DIR ?= build/$(config)
COVERAGE_DIR ?= build/coverage
SRC_DIR ?= $(PACKAGE)
TEST_DIR ?= tests
EXAMPLES_DIR := examples
coverage_binary := $(COVERAGE_DIR)/$(PACKAGE)
unit_tests_binary := $(BUILD_DIR)/ast
integration_tests_binary := $(BUILD_DIR)/tests
docs_dir := build/$(PACKAGE)-docs

SOURCE_FILES := $(shell find $(SRC_DIR) -name '*.pony')
TEST_SOURCE_FILES := $(shell find $(TEST_DIR) -name '*.pony')
EXAMPLES := $(notdir $(shell find $(EXAMPLES_DIR)/* -type d))
EXAMPLES_SOURCE_FILES := $(shell find $(EXAMPLES_DIR) -name '*.pony')
EXAMPLES_BINARIES := $(addprefix $(BUILD_DIR)/,$(EXAMPLES))

ifdef config
	ifeq (,$(filter $(config),debug release))
		$(error Unknown configuration "$(config)")
	endif
endif

ifeq ($(config),release)
	PONYC = $(COMPILE_WITH)
else
	PONYC = $(COMPILE_WITH) --debug
endif

test: unit-tests integration-tests build-examples

build-examples: $(EXAMPLES_BINARIES)

$(EXAMPLES_BINARIES): $(BUILD_DIR)/%: $(SOURCE_FILES) $(EXAMPLES_SOURCE_FILES) | $(BUILD_DIR)
	$(GET_DEPENDENCIES_WITH)
	$(PONYC) -o $(BUILD_DIR) $(EXAMPLES_DIR)/$*

clean:
	$(CLEAN_DEPENDENCIES_WITH)
	rm -rf $(BUILD_DIR)
	rm -rf $(COVERAGE_DIR)


unit-tests: $(unit_tests_binary)
	$^ --sequential --verbose
 
integration-tests: $(integration_tests_binary)
	$^ --sequential

$(unit_tests_binary): $(SOURCE_FILES) | $(BUILD_DIR)
	$(GET_DEPENDENCIES_WITH)
	$(PONYC) -o $(BUILD_DIR) $(SRC_DIR)

$(integration_tests_binary): $(SOURCE_FILES) $(TEST_SOURCE_FILES) | $(BUILD_DIR)
	$(GET_DEPENDENCIES_WITH)
	$(PONYC) -o $(BUILD_DIR) $(TEST_DIR)

$(docs_dir): $(SOURCE_FILES)
	rm -rf $(docs_dir)
	$(GET_DEPENDENCIES_WITH)
	$(PONYC) --docs-public --pass=docs --output build $(SRC_DIR)

docs: $(docs_dir)

coverage: $(coverage_binary)
	kcov --include-pattern="/$(SRC_DIR)/" --exclude-pattern="/test/,_test.pony" $(COVERAGE_DIR) $(coverage_binary)

$(coverage_binary): $(SOURCE_FILES) | $(COVERAGE_DIR)
	$(GET_DEPENDENCIES_WITH)
	$(PONYC) --debug -o $(COVERAGE_DIR) $(SRC_DIR)

TAGS:
	ctags --recurse=yes $(SRC_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(COVERAGE_DIR):
	mkdir -p $(COVERAGE_DIR)

.PHONY: all build-examples clean docs TAGS test coverage
