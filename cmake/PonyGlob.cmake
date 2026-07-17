include_guard(GLOBAL)

# Recursively glob files for use as make prerequisites, dropping any whose path
# holds a character that has no portable spelling in a Makefile prerequisite.
#
# CMake writes such characters in a form GNU make accepts but OpenBSD's base
# make can't parse: a space or '#' comes out backslash-escaped, ':' and other
# make-special characters come out raw. A path holding one can't be a
# prerequisite at all.
#
# Dropping one changes nothing that gets built: these paths only trigger
# rebuilds, and what ponyc compiles comes from the source directory, not from
# this list -- so a dropped path just stops retriggering a rebuild when it
# changes. The paths this drops are test fixtures whose names carry such
# characters to exercise path handling. Do not use this to gather compile
# sources -- a dropped source would silently vanish from the build.
function(pony_prereq_glob out_var)
    file(GLOB_RECURSE _found CONFIGURE_DEPENDS ${ARGN})
    list(FILTER _found EXCLUDE REGEX "[^A-Za-z0-9/._-]")
    set(${out_var} "${_found}" PARENT_SCOPE)
endfunction()
