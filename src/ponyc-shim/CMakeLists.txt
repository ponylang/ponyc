cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(ponyc_shim LANGUAGES C)

# The Windows stable entry point (ponyc-shim.c). One binary, installed under each
# tool's name in <prefix>/bin, that launches the matching versioned binary.
add_executable(ponyc-shim ponyc-shim.c)

# The shim is a tiny standalone launcher, not part of the runtime, so it uses the
# plain CRT string/file functions rather than the _s variants; without this MSVC's
# C4996 "deprecation" fires under warnings-as-errors.
target_compile_definitions(ponyc-shim PRIVATE _CRT_SECURE_NO_WARNINGS)
