cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

file(STRINGS "VERSION" PONYC_PROJECT_VERSION)
project(Pony VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

# Grab the PonyC version number from the "VERSION" source file.
if(NOT DEFINED PONYC_VERSION)
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(PONYC_VERSION "${PONYC_PROJECT_VERSION}-${GIT_REVISION}")
endif()

# Uncomment this to show build commands
# set(CMAKE_VERBOSE_MAKEFILE ON)

# We require C++14 (because LLVM does)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# We require LLVM, Google Test and Google Benchmark
if(NOT PONY_CROSS_LIBPONYRT)
    find_package(LLVM REQUIRED CONFIG PATHS "build/libs/lib/cmake/llvm" "build/libs/lib64/cmake/llvm" NO_DEFAULT_PATH)
    find_package(GTest REQUIRED CONFIG PATHS "build/libs/lib/cmake/GTest" "build/libs/lib64/cmake/GTest" NO_DEFAULT_PATH)
    find_package(benchmark REQUIRED CONFIG PATHS "build/libs/lib/cmake/benchmark" "build/libs/lib64/cmake/benchmark" NO_DEFAULT_PATH)
endif()

# Uses
if(PONY_USE_VALGRIND)
    set(PONY_OUTPUT_SUFFIX "-valgrind")
    add_compile_options(-DUSE_VALGRIND)
endif()

if(PONY_USE_THREAD_SANITIZER)
    set(PONY_OUTPUT_SUFFIX "-thread_sanitizer")
    add_compile_options(-fsanitize=thread -DPONY_SANITIZER=\"thread\")
    add_link_options(-fsanitize=thread -DPONY_SANITIZER=\"thread\")
endif()

if(PONY_USE_ADDRESS_SANITIZER)
    set(PONY_OUTPUT_SUFFIX "-address_sanitizer")
    add_compile_options(-fsanitize=address -DPONY_SANITIZER=\"address\")
    add_link_options(-fsanitize=address -DPONY_SANITIZER=\"address\")
endif()

if(PONY_USE_UNDEFINED_BEHAVIOR_SANITIZER)
    set(PONY_OUTPUT_SUFFIX "-undefined_behavior_sanitizer")
    add_compile_options(-fsanitize=undefined -DPONY_SANITIZER=\"undefined\")
    add_link_options(-fsanitize=undefined -DPONY_SANITIZER=\"undefined\")
endif()

if(PONY_USE_COVERAGE)
    set(PONY_OUTPUT_SUFFIX "-coverage")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-O0 -fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate -fcoverage-mapping)
    else()
        add_compile_options(-O0 -fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs)
    endif()
endif()

if(PONY_USE_POOLTRACK)
    set(PONY_OUTPUT_SUFFIX "-pooltrack")
    add_compile_options(-DUSE_POOLTRACK)
endif()

if(PONY_USE_DTRACE)
    set(PONY_OUTPUT_SUFFIX "-dtrace")
    add_compile_options(-DUSE_DYNAMIC_TRACE)
endif()

if(PONY_USE_MEMTRACK)
    set(PONY_OUTPUT_SUFFIX "-memtrack")
    add_compile_options(-DUSE_MEMTRACK)
endif()

if(PONY_USE_SCHEDULER_SCALING_PTHREADS)
    set(PONY_OUTPUT_SUFFIX "-scheduler_scaling_pthreads")
    add_compile_options(-DUSE_SCHEDULER_SCALING_PTHREADS)
endif()

if(PONY_USE_MEMTRACK_MESSAGES)
    set(PONY_OUTPUT_SUFFIX "-memtrack_messages")
    add_compile_options(-DUSE_MEMTRACK -DUSE_MEMTRACK_MESSAGES)
endif()

# LibPonyC tests assume that our outputs are two directories above the root directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/../debug${PONY_OUTPUT_SUFFIX}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/../release${PONY_OUTPUT_SUFFIX}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/../minsizerel${PONY_OUTPUT_SUFFIX}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/../relwithdebinfo${PONY_OUTPUT_SUFFIX}")

# Libs are now always built in release mode.
set(PONY_LLVM_BUILD_MODE "1")

if(NOT DEFINED PONY_ARCH)
    set(PONY_ARCH "native")
endif()

set(_compiler_arch ${CMAKE_C_COMPILER_ARCHITECTURE_ID})
if("${_compiler_arch}" STREQUAL "")
    set(_compiler_arch ${CMAKE_SYSTEM_PROCESSOR})
endif()

# LLVM component setup
message("Compiler architecture is ${_compiler_arch}")

if(NOT PONY_CROSS_LIBPONYRT)
    set(LLVM_COMPONENTS
        core
        demangle
        objcarcopts
        orcjit
        x86asmparser
        x86codegen
        x86desc
        x86info
        aarch64asmparser
        aarch64codegen
        aarch64desc
        aarch64info
        armasmparser
        armcodegen
        armdesc
        arminfo
    )

    llvm_map_components_to_libnames(PONYC_LLVM_LIBS ${LLVM_COMPONENTS})
    # message("PONYC_LLVM_LIBS: ${PONYC_LLVM_LIBS}")
endif()

# Required definitions.  We use these generators so that the defines are correct for both *nix (where the config applies at configuration time) and Windows (where the config applies at build time).
add_compile_definitions(
    BUILD_COMPILER="${CMAKE_C_COMPILER_VERSION}"
    _FILE_OFFSET_BITS=64
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
    __STDC_LIMIT_MACROS
    LLVM_BUILD_MODE=${PONY_LLVM_BUILD_MODE}
    LLVM_VERSION="${LLVM_VERSION}"
    PONY_COMPILER="${CMAKE_C_COMPILER}"
    PONY_ARCH="${PONY_ARCH}"
    PONY_DEFAULT_PIC=true
    $<$<CONFIG:Debug>:PONY_BUILD_CONFIG="debug">
    $<$<CONFIG:Release>:PONY_BUILD_CONFIG="release">
    $<$<CONFIG:RelWithDebInfo>:PONY_BUILD_CONFIG="release">
    $<$<CONFIG:MinSizeRel>:PONY_BUILD_CONFIG="release">
    PONY_USE_BIGINT
    PONY_VERSION="${PONYC_VERSION}"
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG>
    $<$<CONFIG:MinSizeRel>:NDEBUG>
    $<$<CONFIG:Debug>:PONY_VERSION_STR="${PONYC_VERSION} [debug]\\nCompiled with: LLVM ${LLVM_VERSION} -- ${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}-${_compiler_arch}">
    $<$<CONFIG:Release>:PONY_VERSION_STR="${PONYC_VERSION} [release]\\nCompiled with: LLVM ${LLVM_VERSION} -- ${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}-${_compiler_arch}">
    $<$<CONFIG:RelWithDebInfo>:PONY_VERSION_STR="${PONYC_VERSION} [relwithdebinfo]\\nCompiled with: LLVM ${LLVM_VERSION} -- ${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}-${_compiler_arch}">
    $<$<CONFIG:MinSizeRel>:PONY_VERSION_STR="${PONYC_VERSION} [minsizerel]\\nCompiled with: LLVM ${LLVM_VERSION} -- ${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}-${_compiler_arch}">
)

include(CheckIPOSupported)
if(PONY_USE_LTO)
    check_ipo_supported(RESULT _ipo_supported OUTPUT error)
endif()

if(_ipo_supported)
    message("Configuring with IPO/LTO")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    add_compile_definitions(PONY_USE_LTO)
endif()

if(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    # Disable some warnings for Windows, and compile Debug builds with the regular CRT.
    add_compile_options(
        /wd4142
        /wd4996
        $<$<CONFIG:Debug>:/MD>
        /WX
        /EHa
    )
    add_compile_definitions(
        _MBCS
        _CRT_SECURE_NO_WARNINGS
        $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
    )
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -Werror -Wconversion -Wno-sign-conversion -Wextra -Wall -fpic -fexceptions")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -fno-rtti -fpic -fexceptions")
    add_link_options(-rdynamic)
endif()

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.14 -DUSE_SCHEDULER_SCALING_PTHREADS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -mmacosx-version-min=10.14")
endif()

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "DragonFly")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/cxx_atomics")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem /usr/local/cxx_atomics")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem /usr/local/cxx_atomics")
endif()

set(CMAKE_STATIC_LIBRARY_PREFIX "")

if (PONY_CROSS_LIBPONYRT)
    add_subdirectory(src/libponyrt)
    install(TARGETS libponyrt)
else()
    add_subdirectory(src/libponyc)
    add_subdirectory(src/libponyrt)
    add_subdirectory(src/ponyc)

    add_subdirectory(test/libponyc)
    add_subdirectory(test/libponyrt)

    add_subdirectory(benchmark/libponyc)
    add_subdirectory(benchmark/libponyrt)

    install(TARGETS
        ponyc
        libponyc.tests
        libponyc.benchmarks
        libponyrt.benchmarks
        libponyrt.tests
        DESTINATION bin)
    install(TARGETS
        libponyrt
        DESTINATION lib)
    install(DIRECTORY packages/ DESTINATION packages)
    install(DIRECTORY examples/ DESTINATION examples PATTERN .gitignore EXCLUDE)
endif()
