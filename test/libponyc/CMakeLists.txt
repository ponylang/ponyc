cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(libponyc.tests VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

add_executable(libponyc.tests
    annotations.cc
    array.cc
    badpony.cc
    bare.cc
    buildflagset.cc
    cap.cc
    chain.cc
    codegen.cc
    codegen_ffi.cc
    codegen_final.cc
    codegen_identity.cc
    codegen_optimisation.cc
    codegen_trace.cc
    compiler_serialisation.cc
    dontcare.cc
    ffi.cc
    finalisers.cc
    flatten.cc
    id.cc
    iftype.cc
    lambda.cc
    lexer.cc
    lexint.cc
    literal_inference.cc
    literal_limits.cc
    local_inference.cc
    matchtype.cc
    object.cc
    option_parser.cc
    paint.cc
    parse_entity.cc
    parse_expr.cc
    program.cc
    reach.cc
    recover.cc
    refer.cc
    scope.cc
    signature.cc
    sugar.cc
    sugar_expr.cc
    sugar_traits.cc
    suggest_alt_name.cc
    symtab.cc
    token.cc
    traits.cc
    type_check_bind.cc
    type_check_subtype.cc
    use.cc
    util.cc
    verify.cc
)

target_compile_definitions(libponyc.tests PRIVATE
    PONY_ALWAYS_ASSERT
)

target_include_directories(libponyc.tests
    PRIVATE ../../src/common
    PRIVATE ../../src/libponyc
    PRIVATE ../../build/libs/include
)

set(PONY_PACKAGES_DIR "${CMAKE_SOURCE_DIR}/packages")

if (MSVC)
    string(REPLACE "/" "\\\\" PONY_PACKAGES_DIR ${PONY_PACKAGES_DIR})
endif (MSVC)

target_compile_definitions(libponyc.tests
    PRIVATE PONY_PACKAGES_DIR="${PONY_PACKAGES_DIR}"
)

target_link_directories(libponyc.tests
    PRIVATE ../../build/libs/lib
)

if (MSVC)
    target_link_libraries(libponyc.tests
        PRIVATE libponyc
        PRIVATE libponyrt
        PRIVATE blake2
        PRIVATE GTest::gtest
        PRIVATE dbghelp
        PRIVATE ucrt
        PRIVATE Ws2_32
        PRIVATE Shlwapi
    )

    set_source_files_properties(main.c PROPERTIES LANGUAGE CXX)
    set_property(TARGET libponyc.tests PROPERTY LINK_OPTIONS "/WHOLEARCHIVE:libponyrt.lib")
else()
    if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
        target_link_libraries(libponyc.tests PRIVATE
            -Wl,-force_load,../../src/libponyrt/libponyrt.a
            libponyc
            blake2
            GTest::gtest
        )
    else()
        target_link_libraries(libponyc.tests PRIVATE
            -Wl,--whole-archive
            libponyrt
            -Wl,--no-whole-archive
            libponyc
            blake2
            GTest::gtest
        )
    endif()

    if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
        target_link_libraries(libponyc.tests PRIVATE dl)
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD")
        target_link_libraries(libponyc.tests PRIVATE execinfo)
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "DragonFly")
        target_link_libraries(libponyc.tests PRIVATE execinfo atomic)
    else()
        target_link_libraries(libponyc.tests PRIVATE atomic dl)
        target_link_options(libponyc.tests PRIVATE "-static-libstdc++")
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(libponyc.tests PRIVATE "-static-libgcc")
    endif()

endif (MSVC)

if(NOT PONY_CROSS_LIBPONYRT)
    target_link_libraries(libponyc.tests PRIVATE ${PONYC_LLVM_LIBS})
endif()
