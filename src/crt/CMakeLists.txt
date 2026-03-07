cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(ponyc_crt LANGUAGES C)

set(CRT_SRC_DIR ${CMAKE_SOURCE_DIR}/lib/llvm/src/compiler-rt/lib/builtins)
set(CRT_COMMON_FLAGS -DCRT_HAS_INITFINI_ARRAY -DEH_USE_FRAME_REGISTRY)

# PIE/shared variants (for dynamic linking — the default)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/crtbeginS.o
    COMMAND ${CMAKE_C_COMPILER} -c ${PONY_PIC_FLAG} ${CRT_COMMON_FLAGS}
        -o ${CMAKE_CURRENT_BINARY_DIR}/crtbeginS.o
        ${CRT_SRC_DIR}/crtbegin.c
    DEPENDS ${CRT_SRC_DIR}/crtbegin.c
    COMMENT "Compiling crtbeginS.o (PIC)")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/crtendS.o
    COMMAND ${CMAKE_C_COMPILER} -c ${PONY_PIC_FLAG} ${CRT_COMMON_FLAGS}
        -o ${CMAKE_CURRENT_BINARY_DIR}/crtendS.o
        ${CRT_SRC_DIR}/crtend.c
    DEPENDS ${CRT_SRC_DIR}/crtend.c
    COMMENT "Compiling crtendS.o (PIC)")

# Static variants (for --static linking)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/crtbeginT.o
    COMMAND ${CMAKE_C_COMPILER} -c ${CRT_COMMON_FLAGS}
        -o ${CMAKE_CURRENT_BINARY_DIR}/crtbeginT.o
        ${CRT_SRC_DIR}/crtbegin.c
    DEPENDS ${CRT_SRC_DIR}/crtbegin.c
    COMMENT "Compiling crtbeginT.o (static)")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/crtend.o
    COMMAND ${CMAKE_C_COMPILER} -c ${CRT_COMMON_FLAGS}
        -o ${CMAKE_CURRENT_BINARY_DIR}/crtend.o
        ${CRT_SRC_DIR}/crtend.c
    DEPENDS ${CRT_SRC_DIR}/crtend.c
    COMMENT "Compiling crtend.o (static)")

add_custom_target(crt_objects ALL
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/crtbeginS.o
        ${CMAKE_CURRENT_BINARY_DIR}/crtendS.o
        ${CMAKE_CURRENT_BINARY_DIR}/crtbeginT.o
        ${CMAKE_CURRENT_BINARY_DIR}/crtend.o)

# Copy to output directory alongside libponyrt.
set(CRT_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/crtbeginS.o
    ${CMAKE_CURRENT_BINARY_DIR}/crtendS.o
    ${CMAKE_CURRENT_BINARY_DIR}/crtbeginT.o
    ${CMAKE_CURRENT_BINARY_DIR}/crtend.o)

foreach(CRT_FILE ${CRT_FILES})
    add_custom_command(TARGET crt_objects POST_BUILD
        COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy ${CRT_FILE} ${CMAKE_BINARY_DIR}/../debug${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy ${CRT_FILE} ${CMAKE_BINARY_DIR}/../release${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy ${CRT_FILE} ${CMAKE_BINARY_DIR}/../relwithdebinfo${PONY_OUTPUT_SUFFIX}/
        COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy ${CRT_FILE} ${CMAKE_BINARY_DIR}/../minsizerel${PONY_OUTPUT_SUFFIX}/
    )
endforeach()
