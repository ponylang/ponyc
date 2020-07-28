cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(libponyc VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

add_library(libponyc STATIC
    ponyc.c
    ponydoc.c
    ast/ast.c
    ast/bnfprint.c
    ast/error.c
    ast/frame.c
    ast/id.c
    ast/lexer.c
    ast/lexint.c
    ast/parser.c
    ast/parserapi.c
    ast/printbuf.c
    ast/source.c
    ast/stringtab.c
    ast/symtab.c
    ast/token.c
    ast/treecheck.c
    codegen/codegen.c
    codegen/genbox.c
    codegen/gencall.c
    codegen/gencontrol.c
    codegen/gendesc.c
    codegen/genexe.c
    codegen/genexpr.c
    codegen/genfun.c
    codegen/genheader.c
    codegen/genident.c
    codegen/genlib.c
    codegen/genmatch.c
    codegen/genname.c
    codegen/genobj.c
    codegen/genoperator.c
    codegen/genprim.c
    codegen/genreference.c
    codegen/genserialise.c
    codegen/gentrace.c
    codegen/gentype.c
    codegen/gendebug.cc
    codegen/genjit.cc
    codegen/genjit_old.cc
    codegen/genopt.cc
    codegen/genopt_old.cc
    codegen/host.cc
    expr/array.c
    expr/call.c
    expr/control.c
    expr/ffi.c
    expr/lambda.c
    expr/literal.c
    expr/match.c
    expr/operator.c
    expr/postfix.c
    expr/reference.c
    options/options.c
    pass/docgen.c
    pass/expr.c
    pass/finalisers.c
    pass/flatten.c
    pass/import.c
    pass/names.c
    pass/pass.c
    pass/refer.c
    pass/scope.c
    pass/serialisers.c
    pass/sugar.c
    pass/syntax.c
    pass/traits.c
    pass/verify.c
    pkg/buildflagset.c
    pkg/ifdef.c
    pkg/package.c
    pkg/platformfuns.c
    pkg/program.c
    pkg/use.c
    platform/paths.c
    platform/vcvars.c
    plugin/plugin.c
    reach/paint.c
    reach/reach.c
    reach/subtype.c
    type/alias.c
    type/assemble.c
    type/cap.c
    type/compattype.c
    type/lookup.c
    type/matchtype.c
    type/reify.c
    type/safeto.c
    type/sanitise.c
    type/subtype.c
    type/typeparam.c
    type/viewpoint.c
    verify/call.c
    verify/control.c
    verify/fun.c
    verify/type.c
)

target_compile_definitions(libponyc PRIVATE
    PONY_ALWAYS_ASSERT
)

target_include_directories(libponyc
    PUBLIC ../common
    PRIVATE ../../build/libs/include
    PRIVATE ../../lib/blake2
)

if (MSVC)
    file(GLOB_RECURSE CFILES "${PROJECT_SOURCE_DIR}/*.c")
    set_source_files_properties(${CFILES} PROPERTIES LANGUAGE CXX)
endif (MSVC)

if (NOT MSVC)
    add_custom_command(TARGET libponyc POST_BUILD
        COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_BINARY_DIR}/libponyc.a ${CMAKE_BINARY_DIR}/../debug/
        COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_BINARY_DIR}/libponyc.a ${CMAKE_BINARY_DIR}/../release/
        COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_BINARY_DIR}/libponyc.a ${CMAKE_BINARY_DIR}/../relwithdebinfo/
        COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_BINARY_DIR}/libponyc.a ${CMAKE_BINARY_DIR}/../minsizerel/
    )
endif (NOT MSVC)

# build a standalone version of libponyc.a with all needed dependencies linked statically
if (MSVC)
    # TODO
    #file(GLOB_RECURSE LLVM_OBJS "${PROJECT_SOURCE_DIR}/../../build/build_libs/llvm/src/llvm/Release/lib/*.lib")
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    # TODO
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD")
    # TODO
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "DragonFly")
    # TODO
else()
    # add a rule to generate the standalone library if needed
    add_custom_command(OUTPUT libponyc-standalone.a
        COMMAND cp `find /usr/lib/ -name 'libstdc++.a' -print -quit` libstdcpp.a
        COMMAND echo "create libponyc-standalone.a" > standalone.mri
        COMMAND echo "addlib ${PROJECT_SOURCE_DIR}/../../build/libs/lib/libblake2.a" >> standalone.mri
        COMMAND echo "addlib libstdcpp.a" >> standalone.mri
        COMMAND find ${PROJECT_SOURCE_DIR}/../../build/libs/ -name "libLLVM*.a" | xargs -I % -n 1 echo 'addlib %' >> standalone.mri
        COMMAND echo "addlib $<TARGET_FILE:libponyc>" >> standalone.mri
        COMMAND echo "save" >> standalone.mri
        COMMAND echo "end" >> standalone.mri
        COMMAND ar -M < standalone.mri
        DEPENDS $<TARGET_FILE:libponyc> ${STANDALONE_ARCHIVES}
    )
    # add a separate target that depends on the standalone library file
    add_custom_target(libponyc-standalone ALL
        DEPENDS libponyc
        SOURCES libponyc-standalone.a
    )
    # copy the generated file after it is built
    add_custom_command(TARGET libponyc-standalone POST_BUILD
        COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../debug/
        COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../release/
        COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../relwithdebinfo/
        COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../minsizerel/
    )
endif (MSVC)
