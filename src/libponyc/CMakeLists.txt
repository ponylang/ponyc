cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

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
    codegen/genexe.cc
    codegen/genexpr.c
    codegen/genfun.c
    codegen/genheader.c
    codegen/genident.c
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
    codegen/genopt.cc
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
    pass/completeness.c
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
    pkg/program.cc
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
target_link_libraries(libponyc PRIVATE
    lldMachO
    lldCommon
    lldELF
    lldWasm
)

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(libponyc PRIVATE lldMachO)
elseif(MSVC)
    target_link_libraries(libponyc PRIVATE lldCOFF)
else()
    target_link_libraries(libponyc PRIVATE lldELF)
endif()

target_include_directories(libponyc
    PUBLIC ../common
    PRIVATE ../../build/libs/include
    PRIVATE ../../lib/blake2
)

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET ${PONY_OSX_PLATFORM})
endif()

if (MSVC)
    file(GLOB_RECURSE CFILES "${PROJECT_SOURCE_DIR}/*.c")
    set_source_files_properties(${CFILES} PROPERTIES LANGUAGE CXX)
endif (MSVC)

if (NOT MSVC)
    add_custom_command(TARGET libponyc POST_BUILD
        COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_BINARY_DIR}/libponyc.a ${CMAKE_BINARY_DIR}/../debug${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_BINARY_DIR}/libponyc.a ${CMAKE_BINARY_DIR}/../release${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_BINARY_DIR}/libponyc.a ${CMAKE_BINARY_DIR}/../relwithdebinfo${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_BINARY_DIR}/libponyc.a ${CMAKE_BINARY_DIR}/../minsizerel${PONY_OUTPUT_SUFFIX}/
    )
endif (NOT MSVC)

# build a standalone version of libponyc.a with all needed dependencies linked statically
if (MSVC)
    # this will not include LLVM-C.lib as it contains lots of duplicated definitions
    file(GLOB_RECURSE LLVM_OBJS "${PROJECT_SOURCE_DIR}/../../build/libs/lib/LLVM[!-]*.lib")

    set(libponyc_standalone_file "${CMAKE_STATIC_LIBRARY_PREFIX}ponyc-standalone${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(libponyc_standalone_path "${CMAKE_BINARY_DIR}/${libponyc_standalone_file}")
    add_custom_target(libponyc-standalone ALL
        # CMAKE_AR finds the appropriate version of lib.exe according to desired target and host architecture
        COMMAND ${CMAKE_AR} /NOLOGO /VERBOSE /OUT:${libponyc_standalone_path} "$<TARGET_FILE:libponyc>" ${LLVM_OBJS} "${PROJECT_SOURCE_DIR}/../../build/libs/lib/blake2.lib"
        DEPENDS libponyc)
    add_custom_command(TARGET libponyc-standalone POST_BUILD
        COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_standalone_path} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/${libponyc_standalone_file}
        COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_standalone_path} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${libponyc_standalone_file}
        COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_standalone_path} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO}/${libponyc_standalone_file}
        COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy ${libponyc_standalone_path} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL}/${libponyc_standalone_file}
    )
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    # on macos we dont have no static libc++
    # so when we want to use this lib
    # we need to additionally link libc++ and libz (for llvm)
    file(GLOB_RECURSE LLVM_OBJS "${PROJECT_SOURCE_DIR}/../../build/libs/lib/libLLVM*.a")
    add_custom_target(libponyc-standalone ALL
        COMMAND libtool -static -o libponyc-standalone.a $<TARGET_FILE:libponyc> ${PROJECT_SOURCE_DIR}/../../build/libs/lib/libblake2.a ${LLVM_OBJS}
        DEPENDS $<TARGET_FILE:libponyc> ${STANDALONE_ARCHIVES}
    )
    # copy the generated file after it is built
    add_custom_command(TARGET libponyc-standalone POST_BUILD
        COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../debug${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../release${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../relwithdebinfo${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../minsizerel${PONY_OUTPUT_SUFFIX}/
    )
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "FreeBSD")
    # add a rule to generate the standalone library if needed
    add_custom_command(OUTPUT libponyc-standalone.a
        COMMAND cp `${CMAKE_CXX_COMPILER} --print-file-name='libc++.a'` libcpp.a
        COMMAND echo "create libponyc-standalone.a" > standalone.mri
        COMMAND echo "addlib ${PROJECT_SOURCE_DIR}/../../build/libs/lib/libblake2.a" >> standalone.mri
        COMMAND echo "addlib libcpp.a" >> standalone.mri
        COMMAND find ${PROJECT_SOURCE_DIR}/../../build/libs/ -name "libLLVM*.a" | xargs -I % -n 1 echo 'addlib %' >> standalone.mri
        COMMAND find ${PROJECT_SOURCE_DIR}/../../build/libs/ -name "liblld*.a" | xargs -I % -n 1 echo 'addlib %' >> standalone.mri
        COMMAND echo "addlib $<TARGET_FILE:libponyc>" >> standalone.mri
        COMMAND echo "save" >> standalone.mri
        COMMAND echo "end" >> standalone.mri
        COMMAND ${CMAKE_AR} -M < standalone.mri
        DEPENDS $<TARGET_FILE:libponyc> ${STANDALONE_ARCHIVES}
    )
    # add a separate target that depends on the standalone library file
    add_custom_target(libponyc-standalone ALL
        DEPENDS libponyc
        SOURCES libponyc-standalone.a
    )
    # copy the generated file after it is built
    add_custom_command(TARGET libponyc-standalone POST_BUILD
        COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../debug${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../release${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../relwithdebinfo${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../minsizerel${PONY_OUTPUT_SUFFIX}/
    )
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "OpenBSD")
    # TODO
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "DragonFly")
    # TODO
else()
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libstdc++.a
        OUTPUT_VARIABLE LIBSTDCXX_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(LIBSTDCXX_PATH STREQUAL "libstdc++.a")
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libc++.a
            OUTPUT_VARIABLE LIBCXX_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        if(LIBCXX_PATH STREQUAL "libc++.a")
            message(FATAL_ERROR "Cannot find C++ implementation, looked for libstdc++ and libc++")
        else()
            set(CXX_STDLIB_PATH ${LIBCXX_PATH})
        endif()
    else()
        set(CXX_STDLIB_PATH ${LIBSTDCXX_PATH})
    endif()

    # add a rule to generate the standalone library if needed
    add_custom_command(OUTPUT libponyc-standalone.a
        COMMAND cp ${CXX_STDLIB_PATH} libstdcpp.a
        COMMAND echo "create libponyc-standalone.a" > standalone.mri
        COMMAND echo "addlib ${PROJECT_SOURCE_DIR}/../../build/libs/lib/libblake2.a" >> standalone.mri
        COMMAND echo "addlib libstdcpp.a" >> standalone.mri
        COMMAND find ${PROJECT_SOURCE_DIR}/../../build/libs/ -name "libLLVM*.a" | xargs -I % -n 1 echo 'addlib %' >> standalone.mri
        COMMAND echo "addlib $<TARGET_FILE:libponyc>" >> standalone.mri
        COMMAND echo "save" >> standalone.mri
        COMMAND echo "end" >> standalone.mri
        COMMAND ${CMAKE_AR} -M < standalone.mri
        DEPENDS $<TARGET_FILE:libponyc> ${STANDALONE_ARCHIVES}
    )
    # add a separate target that depends on the standalone library file
    add_custom_target(libponyc-standalone ALL
        DEPENDS libponyc
        SOURCES libponyc-standalone.a
    )
    # copy the generated file after it is built
    add_custom_command(TARGET libponyc-standalone POST_BUILD
        COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../debug${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../release${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../relwithdebinfo${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy libponyc-standalone.a ${CMAKE_BINARY_DIR}/../minsizerel${PONY_OUTPUT_SUFFIX}/
    )
endif (MSVC)
