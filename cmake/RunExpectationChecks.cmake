# ctest wrapper: check that the full-program runner compares a test's
# expected-stdout.txt and expected-compile-errors.txt correctly. Each case is a
# generated test directory run on its own; the case passes when the runner's
# exit status is the one expected and, for a run that must fail, its output
# names the reason. Give each comparison a case where it must fail.
#
# Fixtures are generated here rather than checked in, so programs whose
# expectations are wrong on purpose never sit in a tree a real suite scans.
#
# Args (passed with -D): RUNNER (the runner binary), COMPILER (ponyc),
# FAKE_COMPILER (test/full-program-runner-fixtures/fake-compiler.c, built),
# TEST_LIB, WORKDIR (a scratch dir the fixtures are generated under).

set(_fixture "${WORKDIR}/full-program-runner-expectations")
file(REMOVE_RECURSE "${_fixture}")
file(MAKE_DIRECTORY "${_fixture}/out")

set(_failures "")

# Write a file with exactly the given content (no line-end translation).
function(_put path content)
    file(WRITE "${path}" "${content}")
endfunction()

# Run the runner over TESTS_DIR with the given compiler and extra arguments.
# EXPECT is PASS or FAIL; for FAIL, PHRASE must appear in the runner's output.
function(_check name tests_dir compiler expect phrase)
    set(_args ${ARGN})
    execute_process(
        COMMAND "${RUNNER}" --max_parallel=1 --timeout_s=300
            "--compiler=${compiler}" "--output=${_fixture}/out"
            "--test_lib=${TEST_LIB}" ${_args} "${tests_dir}"
        WORKING_DIRECTORY "${_fixture}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err)
    set(_output "${_out}${_err}")
    if(expect STREQUAL "PASS")
        if(NOT _rc EQUAL 0)
            set(_failures "${_failures}\n${name}: expected the run to pass; exit ${_rc}\n${_output}" PARENT_SCOPE)
        endif()
    else()
        if(_rc EQUAL 0)
            set(_failures "${_failures}\n${name}: expected the run to fail; it passed\n${_output}" PARENT_SCOPE)
        elseif(NOT _output MATCHES "${phrase}")
            set(_failures "${_failures}\n${name}: the run failed without '${phrase}'\n${_output}" PARENT_SCOPE)
        endif()
    endif()
endfunction()

set(_t "${_fixture}/tests")


set(_print_two "actor Main\n  new create(env: Env) =>\n    env.out.print(\"hello\")\n    env.out.print(\"world\")\n")
_put("${_t}/stdout-right/main.pony" "${_print_two}")
_put("${_t}/stdout-right/expected-stdout.txt" "hello\nworld\n")
_put("${_t}/stdout-wrong/main.pony" "${_print_two}")
_put("${_t}/stdout-wrong/expected-stdout.txt" "hello\n")
_put("${_t}/stdout-crlf-file/main.pony" "${_print_two}")
_put("${_t}/stdout-crlf-file/expected-stdout.txt" "hello\r\nworld\r\n")
_put("${_t}/stdout-crlf-program/main.pony"
    "actor Main\n  new create(env: Env) =>\n    env.out.write(\"hello\\r\\nworld\\r\\n\")\n")
_put("${_t}/stdout-crlf-program/expected-stdout.txt" "hello\nworld\n")

set(_undefined "actor Main\n  new create(env: Env) =>\n    foo()\n")
set(_undefined_error "main.pony:3:5: can't find declaration of 'foo'")
_put("${_t}/errors-right/main.pony" "${_undefined}")
_put("${_t}/errors-right/expected-compile-errors.txt" "${_undefined_error}\n")
_put("${_t}/errors-crlf-file/main.pony" "${_undefined}")
_put("${_t}/errors-crlf-file/expected-compile-errors.txt" "${_undefined_error}\r\n")
_put("${_t}/errors-wrong-headline/main.pony" "${_undefined}")
_put("${_t}/errors-wrong-headline/expected-compile-errors.txt"
    "main.pony:3:5: can't find declaration of 'bar'\n")
_put("${_t}/errors-wrong-count/main.pony" "${_undefined}")
_put("${_t}/errors-wrong-count/expected-compile-errors.txt"
    "${_undefined_error}\n${_undefined_error}\n")
_put("${_t}/errors-but-compiles/main.pony" "${_print_two}")
_put("${_t}/errors-but-compiles/expected-compile-errors.txt" "${_undefined_error}\n")

# An error in a subpackage is matched by its path relative to the test
# directory.
set(_lib_main "use lib = \"lib\"\nactor Main\n  new create(env: Env) =>\n    lib.Lib()\n")
set(_lib_lib "primitive Lib\n  fun apply() =>\n    foo()\n")
set(_lib_error "lib/lib.pony:3:5: can't find declaration of 'foo'\n")
_put("${_t}/errors-subpackage/main.pony" "${_lib_main}")
_put("${_t}/errors-subpackage/lib/lib.pony" "${_lib_lib}")
_put("${_t}/errors-subpackage/expected-compile-errors.txt" "${_lib_error}")

# The real compiler's notes on errors in inherited code: a test must list
# them, which holds only while the runner recognises the compiler's words for
# them (see src/libponyc/ast/error.h).
set(_copy_main "trait T\n  fun f(): U8 => \"x\"\nclass C is T\n  new create() => None\nactor Main\n  new create(env: Env) => None\n")
set(_copy_headline "main.pony:2:18: function body isn't the result type\n")
_put("${_t}/note-real-copy-listed/main.pony" "${_copy_main}")
_put("${_t}/note-real-copy-listed/expected-compile-errors.txt"
    "${_copy_headline}${_copy_headline}    main.pony:3:12: in method 'f' inherited by C from T\n")
_put("${_t}/note-real-copy-unlisted/main.pony" "${_copy_main}")
_put("${_t}/note-real-copy-unlisted/expected-compile-errors.txt"
    "${_copy_headline}${_copy_headline}")

set(_blame "${_t}/note-real-blame-unlisted")
_put("${_blame}/main.pony" "use l = \"lib\"\nuse l2 = \"l2\"\nclass C is l.T[l2.Thing]\n  new create() => None\nactor Main\n  new create(env: Env) => env.out.print(C.bar(l2.Thing))\n")
_put("${_blame}/lib/lib.pony" "interface HasY\n  fun y(): String\ninterface Getter\n  fun get(): HasY val\ntrait T[A: Getter val]\n  fun bar(a: A): String => a.get().y()\n")
_put("${_blame}/l2/l2.pony" "primitive _Y\n  fun y(): String => \"y\"\ntype PubY is _Y\nprimitive Thing\n  fun get(): PubY => _Y\n")
_put("${_blame}/expected-compile-errors.txt"
    "lib/lib.pony:6:35: can't lookup fields or methods on private types from other modules\n    main.pony:3:12: in method 'bar' inherited by C from T\n")

# The same program reached through a symlinked test directory: ponyc prints
# the real path, so the runner has to strip the canonical test directory.
# Windows is skipped: creating a symlink there can need privileges.
if(NOT CMAKE_HOST_WIN32)
    set(_real "${_fixture}/real/errors-subpackage")
    _put("${_real}/main.pony" "${_lib_main}")
    _put("${_real}/lib/lib.pony" "${_lib_lib}")
    _put("${_real}/expected-compile-errors.txt" "${_lib_error}")
    file(CREATE_LINK "${_real}" "${_t}/errors-symlinked" SYMBOLIC)
endif()

# Error output from the stand-in compiler, which prints fake-stderr.txt.

set(_err_head "Error:\nmain.pony:2:3: couldn't find 'f'\n    x.f()\n    ^\n")
set(_copy_note "main.pony:9:1: in method 'g' inherited by Bar from Foo")
set(_type_arg_note "main.pony:9:5: '_H' is private to package 'lib' and reaches this code through this type argument, written in package 'main'")
set(_result_note "main.pony:9:5: '_H' is private to package 'lib' and reaches this code through the result type of C's method 'mk', written in package 'main'")
set(_builtin_info "/somewhere/builtin/string.pony:10:3: definition is here")
set(_other_info "main.pony:7:3: method is here")

function(_info_block out)
    set(_block "    Info:\n")
    foreach(_line ${ARGN})
        string(APPEND _block "    ${_line}\n        source\n        ^\n")
    endforeach()
    set(${out} "${_block}" PARENT_SCOPE)
endfunction()

function(_fake name stderr expected)
    _put("${_t}/${name}/main.pony" "actor Main\n  new create(env: Env) => None\n")
    _put("${_t}/${name}/fake-stderr.txt" "${stderr}")
    _put("${_t}/${name}/expected-compile-errors.txt" "${expected}")
endfunction()

_info_block(_b "${_copy_note}")
_fake(note-copy-unlisted "${_err_head}${_b}" "main.pony:2:3: couldn't find 'f'\n")
_fake(note-copy-listed "${_err_head}${_b}"
    "main.pony:2:3: couldn't find 'f'\n    ${_copy_note}\n")
_info_block(_b "${_type_arg_note}")
_fake(note-type-argument-unlisted "${_err_head}${_b}" "main.pony:2:3: couldn't find 'f'\n")
_info_block(_b "${_result_note}")
_fake(note-result-type-unlisted "${_err_head}${_b}" "main.pony:2:3: couldn't find 'f'\n")
_info_block(_b "${_builtin_info}" "${_other_info}")
_fake(info-extra-unlisted "${_err_head}${_b}" "main.pony:2:3: couldn't find 'f'\n")
_fake(info-subsequence "${_err_head}${_b}"
    "main.pony:2:3: couldn't find 'f'\n    ${_other_info}\n")
_fake(info-missing "${_err_head}${_b}"
    "main.pony:2:3: couldn't find 'f'\n    main.pony:8:3: method is here\n")
_info_block(_b "${_other_info}" "${_copy_note}")
_fake(info-out-of-order "${_err_head}${_b}"
    "main.pony:2:3: couldn't find 'f'\n    ${_copy_note}\n    ${_other_info}\n")
_info_block(_b "${_copy_note}")
string(REPLACE "\n" "\r\n" _crlf "${_err_head}${_b}")
_fake(output-crlf "${_crlf}"
    "main.pony:2:3: couldn't find 'f'\n    ${_copy_note}\n")

# No fake-stderr.txt: the stand-in compiler crashes.
_put("${_t}/compiler-crash/main.pony" "actor Main\n  new create(env: Env) => None\n")
_put("${_t}/compiler-crash/expected-compile-errors.txt" "${_undefined_error}\n")


_check(stdout-right "${_t}" "${COMPILER}" PASS "" --only=stdout-right)
_check(stdout-wrong "${_t}" "${COMPILER}" FAIL "stdout differs" --only=stdout-wrong)
_check(stdout-crlf-file "${_t}" "${COMPILER}" PASS "" --only=stdout-crlf-file)
_check(stdout-crlf-program "${_t}" "${COMPILER}" PASS "" --only=stdout-crlf-program)
_check(errors-right "${_t}" "${COMPILER}" PASS "" --only=errors-right)
_check(errors-crlf-file "${_t}" "${COMPILER}" PASS "" --only=errors-crlf-file)
_check(errors-wrong-headline "${_t}" "${COMPILER}" FAIL "headline differs"
    --only=errors-wrong-headline)
_check(errors-wrong-count "${_t}" "${COMPILER}" FAIL "expected 2 error"
    --only=errors-wrong-count)
_check(errors-but-compiles "${_t}" "${COMPILER}" FAIL "the program compiled"
    --only=errors-but-compiles)
_check(errors-subpackage "${_t}" "${COMPILER}" PASS "" --only=errors-subpackage)
_check(note-real-copy-listed "${_t}" "${COMPILER}" PASS ""
    --only=note-real-copy-listed)
_check(note-real-copy-unlisted "${_t}" "${COMPILER}" FAIL "unlisted note"
    --only=note-real-copy-unlisted)
_check(note-real-blame-unlisted "${_t}" "${COMPILER}" FAIL "unlisted note"
    --only=note-real-blame-unlisted)
if(NOT CMAKE_HOST_WIN32)
    _check(errors-symlinked "${_t}" "${COMPILER}" PASS "" --only=errors-symlinked)
endif()

_check(note-copy-unlisted "${_t}" "${FAKE_COMPILER}" FAIL "unlisted note"
    --only=note-copy-unlisted)
_check(note-copy-listed "${_t}" "${FAKE_COMPILER}" PASS "" --only=note-copy-listed)
_check(note-type-argument-unlisted "${_t}" "${FAKE_COMPILER}" FAIL "unlisted note"
    --only=note-type-argument-unlisted)
_check(note-result-type-unlisted "${_t}" "${FAKE_COMPILER}" FAIL "unlisted note"
    --only=note-result-type-unlisted)
_check(info-extra-unlisted "${_t}" "${FAKE_COMPILER}" PASS "" --only=info-extra-unlisted)
_check(info-subsequence "${_t}" "${FAKE_COMPILER}" PASS "" --only=info-subsequence)
_check(info-missing "${_t}" "${FAKE_COMPILER}" FAIL "Info line missing"
    --only=info-missing)
_check(info-out-of-order "${_t}" "${FAKE_COMPILER}" FAIL "Info line missing or out of order"
    --only=info-out-of-order)
_check(output-crlf "${_t}" "${FAKE_COMPILER}" PASS "" --only=output-crlf)
_check(compiler-crash "${_t}" "${FAKE_COMPILER}" FAIL "compiler crashed"
    --only=compiler-crash)
_check(only-unmatched "${_t}" "${COMPILER}" FAIL "--only names no test"
    --only=no-such-test)

# Directories that are not valid tests fail the whole run at discovery.

_put("${_fixture}/broken-both/t/main.pony" "${_print_two}")
_put("${_fixture}/broken-both/t/expected-stdout.txt" "hello\nworld\n")
_put("${_fixture}/broken-both/t/expected-compile-errors.txt" "${_undefined_error}\n")
_check(broken-stdout-and-errors "${_fixture}/broken-both" "${COMPILER}" FAIL
    "could not be configured")
_put("${_fixture}/broken-exit/t/main.pony" "${_undefined}")
_put("${_fixture}/broken-exit/t/expected-exit-code.txt" "0\n")
_put("${_fixture}/broken-exit/t/expected-compile-errors.txt" "${_undefined_error}\n")
_check(broken-exit-and-errors "${_fixture}/broken-exit" "${COMPILER}" FAIL
    "could not be configured")
_put("${_fixture}/broken-info-first/t/main.pony" "${_undefined}")
_put("${_fixture}/broken-info-first/t/expected-compile-errors.txt"
    "    main.pony:1:1: an Info line with no headline\n${_undefined_error}\n")
_check(broken-info-before-headline "${_fixture}/broken-info-first" "${COMPILER}"
    FAIL "an Info line comes before any headline")
_put("${_fixture}/broken-no-errors/t/main.pony" "${_undefined}")
_put("${_fixture}/broken-no-errors/t/expected-compile-errors.txt" "\n")
_check(broken-errors-file-lists-none "${_fixture}/broken-no-errors"
    "${COMPILER}" FAIL "lists no errors")

if(NOT _failures STREQUAL "")
    message(FATAL_ERROR "full-program runner expectation checks failed:${_failures}")
endif()
