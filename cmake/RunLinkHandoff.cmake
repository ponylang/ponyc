# ctest wrapper: check how the links of a build reach ponylink on POSIX.
#
# ponyc (the driver) starts the compiler with a pipe. The compiler runs
# ponylink itself, as its child, for every package but the last; it hands the
# last package's link back over the pipe, and the driver runs ponylink for it
# once the compiler has exited. Run directly, without the pipe, the compiler
# runs every link itself. ponylink removes the bitcode and other temporary
# files, removes C shim objects after a successful link, and moves the mutation
# info into place. At -V3, the compiler prints its process ID and its parent's
# and, for each link, whether it runs the link or hands it off; ponylink prints
# its process ID and its parent's. A handed-off link's ponylink has the
# compiler's parent (the driver) as its parent.
#
# Args (passed with -D): PONYC (the driver), PONYC_COMPILER (the compiler
# binary), PONYLINK (the ponylink binary), LAUNCHER
# (test/ponyc-driver/launcher.c), STUB (test/ponyc-driver/stub.c), WORKDIR (a
# scratch dir the fixture is generated under), APPLE (ON on macOS, where
# ponylink also rebuilds the dSYM bundle), LINUX (ON on Linux).

set(_fixture "${WORKDIR}/link-handoff")
file(REMOVE_RECURSE "${_fixture}")

# Package a links a C shim object, so its removal after a successful link is
# checked.
file(MAKE_DIRECTORY "${_fixture}/a")
file(WRITE "${_fixture}/a/shim.c"
    "#include <stdint.h>\nint32_t link_handoff_shim(void) { return 0; }\n")
file(WRITE "${_fixture}/a/main.pony"
    "use @link_handoff_shim[I32]()\n"
    "actor Main\n  new create(env: Env) =>\n"
    "    env.exitcode(@link_handoff_shim())\n"
    "    env.out.print(\"a\")\n")
file(MAKE_DIRECTORY "${_fixture}/b")
file(WRITE "${_fixture}/b/main.pony"
    "actor Main\n  new create(env: Env) => env.out.print(\"b\")\n")
# badlink can't link; its C shim object must survive the failed link.
file(MAKE_DIRECTORY "${_fixture}/badlink")
file(WRITE "${_fixture}/badlink/shim.c"
    "#include <stdint.h>\nint32_t badlink_shim(void) { return 0; }\n")
file(WRITE "${_fixture}/badlink/main.pony"
    "use \"lib:ponylinkhandoffmissing\"\n"
    "use @badlink_shim[I32]()\n"
    "actor Main\n  new create(env: Env) =>\n"
    "    env.exitcode(@badlink_shim())\n")

# A type error: the build fails before any link.
file(MAKE_DIRECTORY "${_fixture}/typeerror")
file(WRITE "${_fixture}/typeerror/main.pony"
    "actor Main\n  new create(env: Env) =>\n    let x: U8 = \"not a number\"\n")

set(_compiler_line "compiler pid ([0-9]+), parent ([0-9]+)")
set(_ponylink_line "ponylink pid ([0-9]+), parent ([0-9]+)")
set(_handoff_line "Handing the link of [^\n]* to ponyc")
set(_in_compiler_line "Linking [^\n]* in the compiler")
set(_link_failed "ponylink failed \\(exit [0-9]+\\); check linker errors above")

function(run_ponyc _out _dir)
    execute_process(
        COMMAND "${PONYC}" ${ARGN}
        WORKING_DIRECTORY "${_dir}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    set(${_out}_rc "${_rc}" PARENT_SCOPE)
    set(${_out}_text "${_stdout}${_stderr}" PARENT_SCOPE)
endfunction()

function(run_compiler _out _dir)
    execute_process(
        COMMAND "${PONYC_COMPILER}" ${ARGN}
        WORKING_DIRECTORY "${_dir}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    set(${_out}_rc "${_rc}" PARENT_SCOPE)
    set(${_out}_text "${_stdout}${_stderr}" PARENT_SCOPE)
endfunction()

# Run a command under the launcher; _out_text holds the output, which ends with
# how the command ended.
function(run_launched _out _dir)
    execute_process(
        COMMAND "${LAUNCHER}" ${ARGN}
        WORKING_DIRECTORY "${_dir}"
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    set(${_out}_text "${_stdout}${_stderr}" PARENT_SCOPE)
endfunction()

function(fail _name _text)
    message(FATAL_ERROR "${_name}\nOutput:\n${_text}")
endfunction()

# No temporary link files may be left in an output directory once a link has
# finished, successful or not.
function(check_no_temporaries _name _dir _text)
    file(GLOB _left
        "${_dir}/*.bc" "${_dir}/*.lldargs"
        "${_dir}/*.tmp" "${_dir}/*.lto_objects")
    if(_left)
        fail("${_name}: left behind ${_left}" "${_text}")
    endif()
endfunction()

function(check_no_objects _name _dir _text)
    file(GLOB _left "${_dir}/*.o")
    if(_left)
        fail("${_name}: left C shim objects ${_left}" "${_text}")
    endif()
endfunction()

function(check_runs _name _exe _expected _text)
    execute_process(COMMAND "${_exe}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(NOT _rc EQUAL 0 OR NOT _stdout STREQUAL "${_expected}\n")
        fail("${_name}: ${_exe} exited ${_rc} printing '${_stdout}${_stderr}'"
            "${_text}")
    endif()
endfunction()

# The compiler's process ID and its parent's, from its -V3 line.
function(compiler_pids _name _text)
    if(NOT _text MATCHES "${_compiler_line}")
        fail("${_name}: the compiler didn't report its process ID" "${_text}")
    endif()
    set(_compiler_pid "${CMAKE_MATCH_1}" PARENT_SCOPE)
    set(_compiler_parent "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

# The parent process IDs ponylink reported, in order.
function(ponylink_parents _name _text)
    string(REGEX MATCHALL "${_ponylink_line}" _lines "${_text}")
    if(NOT _lines)
        fail("${_name}: ponylink didn't report its process ID" "${_text}")
    endif()
    set(_parents "")
    foreach(_line IN LISTS _lines)
        string(REGEX REPLACE "${_ponylink_line}" "\\2" _parent "${_line}")
        list(APPEND _parents "${_parent}")
    endforeach()
    set(_ponylink_parents "${_parents}" PARENT_SCOPE)
endfunction()

# The last package's link was handed off: the driver, not the compiler, ran
# its ponylink.
function(check_last_link _name _text)
    if(NOT _text MATCHES "${_handoff_line}")
        fail("${_name}: the compiler didn't hand off the link" "${_text}")
    endif()
    compiler_pids("${_name}" "${_text}")
    ponylink_parents("${_name}" "${_text}")
    list(GET _ponylink_parents -1 _last)
    if(NOT _last STREQUAL _compiler_parent OR _last STREQUAL _compiler_pid)
        fail("${_name}: the last ponylink's parent is ${_last}, not the "
            "driver ${_compiler_parent}" "${_text}")
    endif()
endfunction()

# The first link ran in the compiler: its ponylink's parent is the compiler.
function(check_compiler_link _name _text)
    if(NOT _text MATCHES "${_in_compiler_line}")
        fail("${_name}: the compiler didn't run the link" "${_text}")
    endif()
    compiler_pids("${_name}" "${_text}")
    ponylink_parents("${_name}" "${_text}")
    list(GET _ponylink_parents 0 _first)
    if(NOT _first STREQUAL _compiler_pid)
        fail("${_name}: the first ponylink's parent is ${_first}, not the "
            "compiler ${_compiler_pid}" "${_text}")
    endif()
endfunction()

# One package.
run_ponyc(single "${_fixture}" -V3 -o out-single a)
if(NOT single_rc EQUAL 0)
    fail("single: ponyc exited ${single_rc}" "${single_text}")
endif()
check_last_link("single" "${single_text}")
check_runs("single" "${_fixture}/out-single/a" "a" "${single_text}")
check_no_temporaries("single" "${_fixture}/out-single" "${single_text}")
check_no_objects("single" "${_fixture}/out-single" "${single_text}")

# No package path: ponyc builds the current directory.
run_ponyc(noarg "${_fixture}/b" -V3 -o ../out-noarg)
if(NOT noarg_rc EQUAL 0)
    fail("noarg: ponyc exited ${noarg_rc}" "${noarg_text}")
endif()
check_last_link("noarg" "${noarg_text}")
check_runs("noarg" "${_fixture}/out-noarg/b" "b" "${noarg_text}")
check_no_temporaries("noarg" "${_fixture}/out-noarg" "${noarg_text}")

# Two packages: the compiler links the first itself; the last is handed off as
# for a single package.
run_ponyc(two "${_fixture}" -V3 -o out-two a b)
if(NOT two_rc EQUAL 0)
    fail("two: ponyc exited ${two_rc}" "${two_text}")
endif()
check_compiler_link("two" "${two_text}")
check_last_link("two" "${two_text}")
check_runs("two" "${_fixture}/out-two/a" "a" "${two_text}")
check_runs("two" "${_fixture}/out-two/b" "b" "${two_text}")
check_no_temporaries("two" "${_fixture}/out-two" "${two_text}")
check_no_objects("two" "${_fixture}/out-two" "${two_text}")

# The compiler run directly links its own packages.
run_compiler(direct "${_fixture}" -V3 -o out-direct a)
if(NOT direct_rc EQUAL 0)
    fail("direct: the compiler exited ${direct_rc}" "${direct_text}")
endif()
if(direct_text MATCHES "${_handoff_line}")
    fail("direct: the compiler handed off a link" "${direct_text}")
endif()
check_compiler_link("direct" "${direct_text}")
check_runs("direct" "${_fixture}/out-direct/a" "a" "${direct_text}")
check_no_temporaries("direct" "${_fixture}/out-direct" "${direct_text}")
check_no_objects("direct" "${_fixture}/out-direct" "${direct_text}")

# --pass-timings: the link is handed off like any other.
run_ponyc(timings "${_fixture}" -V3 --pass-timings -o out-timings b)
if(NOT timings_rc EQUAL 0)
    fail("timings: ponyc exited ${timings_rc}" "${timings_text}")
endif()
check_last_link("timings" "${timings_text}")
check_runs("timings" "${_fixture}/out-timings/b" "b" "${timings_text}")

# --pass-timings-json where only the final rename of the report fails (a
# non-empty directory sits at the path): the compiler has already handed the
# last link to the driver, and then fails, so the driver abandons the link and
# no executable is built.
file(MAKE_DIRECTORY "${_fixture}/tjdir/timings.json/blocker")
run_ponyc(timingsjson "${_fixture}" -V3
    "--pass-timings-json=${_fixture}/tjdir/timings.json"
    -o out-timingsjson b)
if(NOT timingsjson_rc EQUAL 255)
    fail("timingsjson: ponyc exited ${timingsjson_rc}, not 255"
        "${timingsjson_text}")
endif()
if(NOT timingsjson_text MATCHES "${_handoff_line}")
    fail("timingsjson: the link wasn't prepared for handing off"
        "${timingsjson_text}")
endif()
if(NOT timingsjson_text MATCHES "failed to write pass-timings JSON file")
    fail("timingsjson: the report didn't fail" "${timingsjson_text}")
endif()
if(timingsjson_text MATCHES "[Aa]ssert")
    fail("timingsjson: an assertion fired" "${timingsjson_text}")
endif()
file(GLOB _left "${_fixture}/out-timingsjson/*")
if(_left)
    fail("timingsjson: left ${_left}" "${timingsjson_text}")
endif()

# --pass-timings with stderr on a pipe that has no reader: printing the table
# ends the compiler by SIGPIPE, after it has handed over the link, so the
# driver removes the link's files.
run_launched(timingspipe "${_fixture}" --stderr-no-reader
    -- "${PONYC}" -V0 --pass-timings --mutate -o out-timingspipe b)
if(NOT timingspipe_text MATCHES "(^|\n)signaled PIPE\n")
    fail("timingspipe: ponyc didn't die of SIGPIPE" "${timingspipe_text}")
endif()
check_no_temporaries("timingspipe" "${_fixture}/out-timingspipe"
    "${timingspipe_text}")

# --pass-timings-json with the last link handed off: the file is written before
# the driver runs the link, so a link that fails leaves a report of a build that succeeded.
run_ponyc(timingsok "${_fixture}"
    "--pass-timings-json=${_fixture}/tjok.json" -o out-timingsok badlink)
if(NOT timingsok_rc EQUAL 255)
    fail("timingsok: ponyc exited ${timingsok_rc}, not 255" "${timingsok_text}")
endif()
if(NOT timingsok_text MATCHES "${_link_failed}")
    fail("timingsok: the link didn't fail" "${timingsok_text}")
endif()
file(READ "${_fixture}/tjok.json" _report)
if(NOT _report MATCHES "\"status\": \"ok\"")
    fail("timingsok: the report doesn't record success" "${_report}")
endif()

# A build that fails before linking: ponyc fails and ponylink never runs.
run_ponyc(typeerror "${_fixture}" -V3 -o out-typeerror typeerror)
if(NOT typeerror_rc EQUAL 255)
    fail("typeerror: ponyc exited ${typeerror_rc}, not 255" "${typeerror_text}")
endif()
if(typeerror_text MATCHES "ponylink pid")
    fail("typeerror: ponylink ran" "${typeerror_text}")
endif()

# Options that stop before compiling, through the driver.
run_ponyc(version "${_fixture}" --version)
if(NOT version_rc EQUAL 0 OR NOT version_text MATCHES "[0-9]+\\.[0-9]+\\.[0-9]+")
    fail("version: ponyc exited ${version_rc}" "${version_text}")
endif()
run_ponyc(help "${_fixture}" --help)
if(NOT help_rc EQUAL 0 OR NOT help_text MATCHES "--pass-timings")
    fail("help: ponyc exited ${help_rc}" "${help_text}")
endif()

# A failed link of the last package: ponylink reports it, ponyc fails, the
# temporary files go, and the C shim object stays.
run_ponyc(fail "${_fixture}" -o out-fail badlink)
if(NOT fail_rc EQUAL 255)
    fail("fail: ponyc exited ${fail_rc}, not 255" "${fail_text}")
endif()
if(NOT fail_text MATCHES "${_link_failed}")
    fail("fail: no link failure message" "${fail_text}")
endif()
if(fail_text MATCHES "(^|\n)ponyc: ")
    fail("fail: the driver printed a message of its own" "${fail_text}")
endif()
check_no_temporaries("fail" "${_fixture}/out-fail" "${fail_text}")
file(GLOB _objects "${_fixture}/out-fail/*.o")
if(NOT _objects)
    fail("fail: the C shim object was removed after a failed link"
        "${fail_text}")
endif()

# A failed link of an earlier package, which the compiler runs itself.
run_ponyc(childfail "${_fixture}" -o out-childfail badlink a)
if(NOT childfail_rc EQUAL 255)
    fail("childfail: ponyc exited ${childfail_rc}, not 255" "${childfail_text}")
endif()
if(NOT childfail_text MATCHES "${_link_failed}")
    fail("childfail: no link failure message" "${childfail_text}")
endif()
if(NOT childfail_text MATCHES "linking [^\n]*badlink failed" OR
    childfail_text MATCHES "internal failure not reported")
    fail("childfail: ponyc didn't record which link failed" "${childfail_text}")
endif()
check_no_temporaries("childfail" "${_fixture}/out-childfail" "${childfail_text}")
file(GLOB _objects "${_fixture}/out-childfail/*.o")
if(NOT _objects)
    fail("childfail: the C shim object was removed after a failed link"
        "${childfail_text}")
endif()

# --mutate: the mutation info ends up next to the executable, whether the
# compiler runs the link or hands it off. Without -V3 neither the compiler nor
# ponylink prints a process ID.
run_ponyc(mutate "${_fixture}" --mutate -o out-mutate b a)
if(NOT mutate_rc EQUAL 0)
    fail("mutate: ponyc exited ${mutate_rc}" "${mutate_text}")
endif()
foreach(_pkg a b)
    if(NOT EXISTS "${_fixture}/out-mutate/${_pkg}.mutants.json")
        fail("mutate: no ${_pkg}.mutants.json" "${mutate_text}")
    endif()
endforeach()
check_no_temporaries("mutate" "${_fixture}/out-mutate" "${mutate_text}")
if(mutate_text MATCHES "pid [0-9]+")
    fail("mutate: printed a process ID without -V3" "${mutate_text}")
endif()

# The mutation info can't be renamed into place (a directory sits there): the
# executable is linked and cleaned up after as usual, and ponyc exits 255.
# out-rename-last has the last package, whose link is handed off;
# out-rename-first has the first package, which the compiler links itself.
foreach(_case last first)
    set(_dir "${_fixture}/out-rename-${_case}")
    file(MAKE_DIRECTORY "${_dir}/a.mutants.json/blocker")
    if(_case STREQUAL "last")
        run_ponyc(rename "${_fixture}" -V3 --mutate -o "out-rename-${_case}" a)
    else()
        run_ponyc(rename "${_fixture}" --mutate -o "out-rename-${_case}" a b)
    endif()
    if(NOT rename_rc EQUAL 255)
        fail("rename-${_case}: ponyc exited ${rename_rc}, not 255"
            "${rename_text}")
    endif()
    if(NOT rename_text MATCHES "couldn't write mutation info file")
        fail("rename-${_case}: no mutation info message" "${rename_text}")
    endif()
    if(rename_text MATCHES "linking [^\n]*failed" OR
        rename_text MATCHES "ponylink failed")
        fail("rename-${_case}: reported a successful link as failed"
            "${rename_text}")
    endif()
    if(_case STREQUAL "last")
        check_last_link("rename-last" "${rename_text}")
    elseif(NOT rename_text MATCHES "linked [^\n]*a without its mutation info")
        fail("rename-first: ponyc didn't report the missing mutation info"
            "${rename_text}")
    endif()
    if(NOT EXISTS "${_dir}/a")
        fail("rename-${_case}: no executable" "${rename_text}")
    endif()
    check_no_temporaries("rename-${_case}" "${_dir}" "${rename_text}")
    check_no_objects("rename-${_case}" "${_dir}" "${rename_text}")
endforeach()

# The mutation info gets the same permissions as any new file created in the
# same directory under the same umask.
file(MAKE_DIRECTORY "${_fixture}/out-umask")
execute_process(
    COMMAND sh -c
        "umask 002 && : > out-umask/reference && exec \"$0\" --mutate -o out-umask b"
        "${PONYC}"
    WORKING_DIRECTORY "${_fixture}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(NOT _rc EQUAL 0)
    fail("umask: ponyc exited ${_rc}" "${_stdout}${_stderr}")
endif()
execute_process(COMMAND ls -l "${_fixture}/out-umask/reference"
    OUTPUT_VARIABLE _ref)
execute_process(COMMAND ls -l "${_fixture}/out-umask/b.mutants.json"
    OUTPUT_VARIABLE _ls)
string(SUBSTRING "${_ref}" 0 10 _ref_mode)
string(SUBSTRING "${_ls}" 0 10 _mode)
if(NOT _mode STREQUAL _ref_mode)
    fail("umask: b.mutants.json has mode ${_mode}, a new file gets ${_ref_mode}"
        "${_stdout}${_stderr}")
endif()

# A symlink where the compiler writes the mutation info before the link: the
# compiler replaces it without writing through it.
file(MAKE_DIRECTORY "${_fixture}/out-mutsym")
file(WRITE "${_fixture}/mutsym-target" "keep\n")
file(CREATE_LINK "${_fixture}/mutsym-target"
    "${_fixture}/out-mutsym/a.mutants.json.tmp" SYMBOLIC)
run_ponyc(mutsym "${_fixture}" --mutate -o out-mutsym a)
if(NOT mutsym_rc EQUAL 0)
    fail("mutsym: ponyc exited ${mutsym_rc}" "${mutsym_text}")
endif()
file(READ "${_fixture}/mutsym-target" _target)
if(NOT _target STREQUAL "keep\n")
    fail("mutsym: ponyc wrote the mutation info through a symlink"
        "${mutsym_text}")
endif()
if(IS_SYMLINK "${_fixture}/out-mutsym/a.mutants.json" OR
    NOT EXISTS "${_fixture}/out-mutsym/a.mutants.json")
    fail("mutsym: a.mutants.json is missing or a symlink" "${mutsym_text}")
endif()

# The mutation info can't be written before the link (a directory sits where
# the compiler writes it): no link runs and the bitcode goes.
file(MAKE_DIRECTORY "${_fixture}/out-tmpfail/a.mutants.json.tmp/blocker")
run_ponyc(tmpfail "${_fixture}" --mutate -o out-tmpfail a)
if(NOT tmpfail_rc EQUAL 255)
    fail("tmpfail: ponyc exited ${tmpfail_rc}, not 255" "${tmpfail_text}")
endif()
if(NOT tmpfail_text MATCHES "couldn't write mutation info file")
    fail("tmpfail: no mutation info message" "${tmpfail_text}")
endif()
file(GLOB _left "${_fixture}/out-tmpfail/*.bc"
    "${_fixture}/out-tmpfail/*.lldargs")
if(_left OR EXISTS "${_fixture}/out-tmpfail/a")
    fail("tmpfail: left ${_left} or linked anyway" "${tmpfail_text}")
endif()

# A symlink already where the compiler writes the linker arguments: the
# compiler replaces it without writing through it.
file(MAKE_DIRECTORY "${_fixture}/out-symlink")
file(WRITE "${_fixture}/symlink-target" "keep\n")
file(CREATE_LINK "${_fixture}/symlink-target"
    "${_fixture}/out-symlink/b.lldargs" SYMBOLIC)
run_ponyc(symlink "${_fixture}" -o out-symlink b)
if(NOT symlink_rc EQUAL 0)
    fail("symlink: ponyc exited ${symlink_rc}" "${symlink_text}")
endif()
file(READ "${_fixture}/symlink-target" _target)
if(NOT _target STREQUAL "keep\n")
    fail("symlink: ponyc wrote through a symlink at its args file"
        "${symlink_text}")
endif()
check_runs("symlink" "${_fixture}/out-symlink/b" "b" "${symlink_text}")

# An output directory whose name contains a line break can't be written into
# the one-argument-per-line .lldargs file ponylink reads.
run_ponyc(newline "${_fixture}" --mutate -o "out-new\nline" b)
if(NOT newline_rc EQUAL 255)
    fail("newline: ponyc exited ${newline_rc}, not 255" "${newline_text}")
endif()
if(NOT newline_text MATCHES "contains a line break")
    fail("newline: no line break message" "${newline_text}")
endif()
check_no_temporaries("newline" "${_fixture}/out-new\nline" "${newline_text}")

# The linker arguments file can't be written (a non-empty directory sits at
# its path): no link runs, and the link's temporary files are removed.
file(MAKE_DIRECTORY "${_fixture}/out-argsfail/b.lldargs/blocker")
run_ponyc(argsfail "${_fixture}" --mutate -o out-argsfail b)
if(NOT argsfail_rc EQUAL 255)
    fail("argsfail: ponyc exited ${argsfail_rc}, not 255" "${argsfail_text}")
endif()
if(NOT argsfail_text MATCHES "failed to write linker args file")
    fail("argsfail: no message about the arguments file" "${argsfail_text}")
endif()
file(GLOB _left "${_fixture}/out-argsfail/*.bc" "${_fixture}/out-argsfail/*.tmp")
if(_left OR EXISTS "${_fixture}/out-argsfail/b")
    fail("argsfail: left ${_left} or linked anyway" "${argsfail_text}")
endif()

# An option ponylink doesn't know: a usage error, and ponylink removes nothing.
set(_sentinel "${_fixture}/sentinel")
file(WRITE "${_sentinel}" "keep\n")
execute_process(
    COMMAND "${PONYLINK}" --ponyc-job --args "${_fixture}/nonexistent.lldargs"
        --remove "${_sentinel}" --bogus
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(NOT _rc EQUAL 1 OR NOT _stderr MATCHES "unknown option --bogus")
    fail("unknown-option: ponylink exited ${_rc}" "${_stdout}${_stderr}")
endif()
if(NOT EXISTS "${_sentinel}")
    fail("unknown-option: ponylink removed a file" "${_stdout}${_stderr}")
endif()

# An option missing its value, or one of its two values: usage errors too.
execute_process(
    COMMAND "${PONYLINK}" --ponyc-job --args "${_fixture}/nonexistent.lldargs"
        --remove "${_sentinel}" --remove
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(NOT _rc EQUAL 1 OR NOT _stderr MATCHES "missing value for --remove")
    fail("missing-value: ponylink exited ${_rc}" "${_stdout}${_stderr}")
endif()
if(NOT EXISTS "${_sentinel}")
    fail("missing-value: ponylink removed a file" "${_stdout}${_stderr}")
endif()
execute_process(
    COMMAND "${PONYLINK}" --ponyc-job --args "${_fixture}/nonexistent.lldargs"
        --remove "${_sentinel}" --mutate "${_fixture}/m.tmp"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(NOT _rc EQUAL 1 OR NOT _stderr MATCHES "missing value for --mutate")
    fail("missing-mutate-value: ponylink exited ${_rc}" "${_stdout}${_stderr}")
endif()
if(NOT EXISTS "${_sentinel}")
    fail("missing-mutate-value: ponylink removed a file" "${_stdout}${_stderr}")
endif()

# No --args: a usage error.
execute_process(
    COMMAND "${PONYLINK}" --ponyc-job --remove "${_sentinel}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(NOT _rc EQUAL 1 OR NOT _stderr MATCHES "missing --args")
    fail("no-args: ponylink exited ${_rc}" "${_stdout}${_stderr}")
endif()
if(NOT EXISTS "${_sentinel}")
    fail("no-args: ponylink removed a file" "${_stdout}${_stderr}")
endif()

# A remove-tree path that is a symlink: ponylink removes the symlink and
# leaves the directory it points to alone.
file(MAKE_DIRECTORY "${_fixture}/tree-target")
file(WRITE "${_fixture}/tree-target/keep" "keep\n")
file(CREATE_LINK "${_fixture}/tree-target" "${_fixture}/tree-link" SYMBOLIC)
execute_process(
    COMMAND "${PONYLINK}" --ponyc-job --args "${_fixture}/nonexistent.lldargs"
        --remove-tree "${_fixture}/tree-link"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(NOT _rc EQUAL 255)
    fail("tree: ponylink exited ${_rc}, not 255" "${_stdout}${_stderr}")
endif()
if(IS_SYMLINK "${_fixture}/tree-link")
    fail("tree: ponylink left the symlink" "${_stdout}${_stderr}")
endif()
if(NOT EXISTS "${_fixture}/tree-target/keep")
    fail("tree: ponylink removed files through a symlink"
        "${_stdout}${_stderr}")
endif()

# --abandon: ponylink removes the job's temporary paths and links nothing.
# Linking these arguments would print lld's version. The tree holds a nested
# directory and a symlink to a directory outside it, which must survive.
file(MAKE_DIRECTORY "${_fixture}/abandon/tree/nested" "${_fixture}/abandon-outside")
file(WRITE "${_fixture}/abandon/tree/nested/file" "x\n")
file(WRITE "${_fixture}/abandon-outside/sentinel" "keep\n")
file(CREATE_LINK "${_fixture}/abandon-outside" "${_fixture}/abandon/tree/link"
    SYMBOLIC)
file(WRITE "${_fixture}/abandon/version.lldargs" "ld.lld\n--version\n")
foreach(_f remove mutate.tmp keep)
    file(WRITE "${_fixture}/abandon/${_f}" "x\n")
endforeach()
execute_process(
    COMMAND "${PONYLINK}" --ponyc-job --abandon
        --args "${_fixture}/abandon/version.lldargs"
        --remove "${_fixture}/abandon/remove"
        --remove-tree "${_fixture}/abandon/tree"
        --mutate "${_fixture}/abandon/mutate.tmp" "${_fixture}/abandon/mutate"
        --remove-on-success "${_fixture}/abandon/keep"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
set(_text "${_stdout}${_stderr}")
if(NOT _rc EQUAL 0)
    fail("abandon: ponylink exited ${_rc}" "${_text}")
endif()
if(_text MATCHES "LLD")
    fail("abandon: ponylink linked" "${_text}")
endif()
foreach(_f remove tree mutate.tmp mutate)
    if(EXISTS "${_fixture}/abandon/${_f}")
        fail("abandon: ${_f} exists" "${_text}")
    endif()
endforeach()
if(NOT EXISTS "${_fixture}/abandon/keep")
    fail("abandon: removed a path only a successful link removes" "${_text}")
endif()
if(NOT EXISTS "${_fixture}/abandon-outside/sentinel")
    fail("abandon: removed files through a symlink in the tree" "${_text}")
endif()

# --abandon --verbose with stderr on a pipe that has no reader: the write fails
# instead of ending ponylink, which still removes the job's files.
file(WRITE "${_fixture}/abandon/nopipe-remove" "x\n")
run_launched(abandonpipe "${_fixture}/abandon" --stderr-no-reader
    -- "${PONYLINK}" --ponyc-job --abandon
    --args "${_fixture}/abandon/version.lldargs" --verbose
    --remove "${_fixture}/abandon/nopipe-remove")
if(NOT abandonpipe_text MATCHES "(^|\n)exited 0\n" OR
    EXISTS "${_fixture}/abandon/nopipe-remove")
    fail("abandonpipe: ponylink didn't remove the job's files"
        "${abandonpipe_text}")
endif()

# Bad handoff options given to the compiler: an error, before anything else
# (--version would otherwise succeed).
# Values that aren't a number above 2 that fits an int are rejected as
# invalid; a number naming a closed fd is rejected as not an open pipe.
foreach(_value abc "" 0 1 2 99 3x 99999999999 +5)
    run_compiler(badhandoff "${_fixture}" "--link-handoff=${_value}" --version)
    if(_value STREQUAL "99")
        set(_expected "--link-handoff=99 is not an open pipe")
    else()
        set(_expected "ponyc-compiler: invalid --link-handoff=${_value}\n")
    endif()
    string(FIND "${badhandoff_text}" "${_expected}" _found)
    if(NOT badhandoff_rc EQUAL 255 OR _found EQUAL -1)
        fail("badhandoff '${_value}': the compiler exited ${badhandoff_rc}"
            "${badhandoff_text}")
    endif()
endforeach()

# A handoff fd that is a regular file rather than a pipe: an error.
run_launched(handofffile "${_fixture}"
    --handoff-file "${_fixture}/handoff-file" -- "${PONYC_COMPILER}" --version)
if(NOT handofffile_text MATCHES "(^|\n)exited 255\n" OR
    NOT handofffile_text MATCHES "is not an open pipe")
    fail("handofffile: the compiler accepted a regular file"
        "${handofffile_text}")
endif()

# A handoff pipe whose reader goes away while the compiler runs: writing the
# job fails, and the compiler removes the link's files and fails. The pipe is
# full before the compiler starts, so writing the job waits until the read end
# is closed, which happens once the compiler has written the mutation info,
# after its startup check of the pipe.
run_launched(epipe "${_fixture}" --handoff-pipe --fill-handoff
    --wait-file out-epipe/b.mutants.json.tmp --close-handoff
    -- "${PONYC_COMPILER}" --mutate -o out-epipe b)
if(NOT epipe_text MATCHES "(^|\n)exited 255\n" OR
    NOT epipe_text MATCHES "unable to hand the link to ponyc")
    fail("epipe: the compiler didn't fail" "${epipe_text}")
endif()
check_no_temporaries("epipe" "${_fixture}/out-epipe" "${epipe_text}")

# A handoff pipe that already has no reader when the compiler starts, as when
# ponyc was killed before the compiler took the pipe (Linux and FreeBSD): the
# compiler exits 255 before compiling anything.
if(LINUX OR CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD")
    run_launched(noreader "${_fixture}" --handoff-closed -- "${PONYC_COMPILER}"
        -o out-noreader b)
    if(NOT noreader_text MATCHES "(^|\n)exited 255\n" OR
        noreader_text MATCHES "Building")
        fail("noreader: the compiler compiled for a pipe with no reader"
            "${noreader_text}")
    endif()
endif()

# A signal sent to ponylink while lld runs: ponylink passes it on to lld,
# removes the link's temporary files, and dies of the same signal. lld blocks
# opening its input, a FIFO, until the launcher opens the FIFO for writing;
# once lld is gone, nothing has the FIFO open for reading.
file(MAKE_DIRECTORY "${_fixture}/sig")
execute_process(COMMAND mkfifo "${_fixture}/sig/input.o" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    fail("sig: can't create a FIFO" "")
endif()
file(WRITE "${_fixture}/sig/link.lldargs"
    "ld.lld\n-o\n${_fixture}/sig/exe\n${_fixture}/sig/input.o\n")
function(run_signaled _name)
    file(MAKE_DIRECTORY "${_fixture}/sig/tree")
    file(WRITE "${_fixture}/sig/remove" "x\n")
    file(WRITE "${_fixture}/sig/mutate.tmp" "x\n")
    run_launched(${_name} "${_fixture}/sig" ${ARGN}
        -- "${PONYLINK}" --ponyc-job --args "${_fixture}/sig/link.lldargs"
        --remove "${_fixture}/sig/remove"
        --remove-tree "${_fixture}/sig/tree"
        --mutate "${_fixture}/sig/mutate.tmp" "${_fixture}/sig/mutate")
    set(_text "${${_name}_text}")
    if(NOT _text MATCHES "(^|\n)signaled TERM\n")
        fail("${_name}: ponylink didn't die of SIGTERM" "${_text}")
    endif()
    if(NOT _text MATCHES "fifo reader gone")
        fail("${_name}: lld outlived ponylink" "${_text}")
    endif()
    foreach(_f remove tree mutate.tmp mutate exe)
        if(EXISTS "${_fixture}/sig/${_f}")
            fail("${_name}: ${_f} exists" "${_text}")
        endif()
    endforeach()
    set(${_name}_text "${_text}" PARENT_SCOPE)
endfunction()

run_signaled(sigterm --fifo-open "${_fixture}/sig/input.o" --signal TERM)

# Started with SIGHUP ignored: SIGHUP leaves ponylink running.
run_signaled(sighup --ignore HUP --fifo-open "${_fixture}/sig/input.o"
    --signal HUP --sleep-ms 300 --check-alive --signal TERM)
if(NOT sighup_text MATCHES "(^|\n)alive\n")
    fail("sighup: an ignored SIGHUP ended ponylink" "${sighup_text}")
endif()

# A signal sent to ponylink after lld succeeded, while it runs dsymutil:
# ponylink passes it on, removes the temporary files, the mutation info it
# hasn't renamed and the mutation info of an earlier build, leaves the
# executable and the paths only a finished link removes, and dies of the
# signal. A dsymutil found first on PATH stands in for the real one: it creates
# a file, waits, and creates another file when it gets SIGTERM. The lld
# arguments only print lld's version, so the executable here is a file the test
# creates.
set(_late "${_fixture}/late")
file(MAKE_DIRECTORY "${_late}/bin" "${_late}/tree")
file(WRITE "${_late}/bin/dsymutil"
    "#!/bin/sh\n"
    "sleep 60 > /dev/null 2>&1 &\n"
    "trap 'kill $!; : > \"$DSYM_GOT\"; exit 143' TERM\n"
    ": > \"$DSYM_READY\"\n"
    "wait\n")
execute_process(COMMAND chmod +x "${_late}/bin/dsymutil")
file(WRITE "${_late}/version.lldargs" "ld.lld\n--version\n")
foreach(_f exe remove mutate.tmp mutate keep)
    file(WRITE "${_late}/${_f}" "x\n")
endforeach()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=${_late}/bin:$ENV{PATH}" "DSYM_READY=${_late}/ready"
        "DSYM_GOT=${_late}/dsymutil-got-term"
        "${LAUNCHER}" --wait-file "${_late}/ready" --signal TERM
        -- "${PONYLINK}" --ponyc-job --args "${_late}/version.lldargs"
        --dsym "${_late}/exe"
        --mutate "${_late}/mutate.tmp" "${_late}/mutate"
        --remove "${_late}/remove"
        --remove-tree "${_late}/tree"
        --remove-on-success "${_late}/keep"
    OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
set(_text "${_stdout}${_stderr}")
if(NOT _text MATCHES "(^|\n)signaled TERM\n")
    fail("late-signal: ponylink didn't die of SIGTERM" "${_text}")
endif()
if(NOT EXISTS "${_late}/dsymutil-got-term")
    fail("late-signal: SIGTERM didn't reach dsymutil" "${_text}")
endif()
foreach(_f remove tree mutate.tmp mutate)
    if(EXISTS "${_late}/${_f}")
        fail("late-signal: ${_f} exists" "${_text}")
    endif()
endforeach()
foreach(_f exe keep)
    if(NOT EXISTS "${_late}/${_f}")
        fail("late-signal: ${_f} was removed" "${_text}")
    endif()
endforeach()

# dsymutil fails after a successful link: ponylink says so and finishes as
# after any successful link.
set(_dsymfail "${_fixture}/dsymfail")
file(MAKE_DIRECTORY "${_dsymfail}/bin" "${_dsymfail}/tree")
file(WRITE "${_dsymfail}/bin/dsymutil" "#!/bin/sh\nexit 1\n")
execute_process(COMMAND chmod +x "${_dsymfail}/bin/dsymutil")
file(WRITE "${_dsymfail}/version.lldargs" "ld.lld\n--version\n")
foreach(_f exe remove mutate.tmp keep)
    file(WRITE "${_dsymfail}/${_f}" "x\n")
endforeach()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${_dsymfail}/bin:$ENV{PATH}"
        "${PONYLINK}" --ponyc-job --args "${_dsymfail}/version.lldargs"
        --dsym "${_dsymfail}/exe"
        --mutate "${_dsymfail}/mutate.tmp" "${_dsymfail}/mutate"
        --remove "${_dsymfail}/remove"
        --remove-tree "${_dsymfail}/tree"
        --remove-on-success "${_dsymfail}/keep"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
set(_text "${_stdout}${_stderr}")
if(NOT _rc EQUAL 0 OR NOT _text MATCHES "unable to create dsym")
    fail("dsymfail: ponylink exited ${_rc}" "${_text}")
endif()
foreach(_f remove tree mutate.tmp keep)
    if(EXISTS "${_dsymfail}/${_f}")
        fail("dsymfail: ${_f} exists" "${_text}")
    endif()
endforeach()
if(NOT EXISTS "${_dsymfail}/exe" OR NOT EXISTS "${_dsymfail}/mutate")
    fail("dsymfail: the executable or the mutation info is missing" "${_text}")
endif()

# dsymutil can't be started (none on PATH) or is killed after a successful
# link: ponylink says so and finishes as after any successful link. The link
# doesn't need PATH: ponylink runs lld from its own path.
foreach(_case notfound killed)
    set(_dsym "${_fixture}/dsym-${_case}")
    file(MAKE_DIRECTORY "${_dsym}/bin" "${_dsym}/tree")
    if(_case STREQUAL "killed")
        file(WRITE "${_dsym}/bin/dsymutil" "#!/bin/sh\nkill -USR1 $$\n")
        execute_process(COMMAND chmod +x "${_dsym}/bin/dsymutil")
        set(_expected "ponylink: dsymutil was killed by signal [0-9]+")
        set(_path "${_dsym}/bin:$ENV{PATH}")
    else()
        set(_expected "ponylink: unable to start dsymutil: ")
        set(_path "${_dsym}/bin")
    endif()
    file(WRITE "${_dsym}/version.lldargs" "ld.lld\n--version\n")
    foreach(_f exe remove mutate.tmp keep)
        file(WRITE "${_dsym}/${_f}" "x\n")
    endforeach()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${_path}"
            "${PONYLINK}" --ponyc-job --args "${_dsym}/version.lldargs"
            --dsym "${_dsym}/exe"
            --mutate "${_dsym}/mutate.tmp" "${_dsym}/mutate"
            --remove "${_dsym}/remove"
            --remove-tree "${_dsym}/tree"
            --remove-on-success "${_dsym}/keep"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    set(_text "${_stdout}${_stderr}")
    if(NOT _rc EQUAL 0 OR NOT _text MATCHES "${_expected}")
        fail("dsym-${_case}: ponylink exited ${_rc}" "${_text}")
    endif()
    foreach(_f remove tree mutate.tmp keep)
        if(EXISTS "${_dsym}/${_f}")
            fail("dsym-${_case}: ${_f} exists" "${_text}")
        endif()
    endforeach()
    if(NOT EXISTS "${_dsym}/exe" OR NOT EXISTS "${_dsym}/mutate")
        fail("dsym-${_case}: the executable or the mutation info is missing"
            "${_text}")
    endif()
endforeach()

# The mutation info can't be renamed into place after a successful link (its
# temporary file is missing): ponylink says so, exits 4, and removes the
# mutation info an earlier build left, which describes another executable.
set(_stale "${_fixture}/stale")
file(MAKE_DIRECTORY "${_stale}")
file(WRITE "${_stale}/version.lldargs" "ld.lld\n--version\n")
file(WRITE "${_stale}/mutate" "x\n")
execute_process(
    COMMAND "${PONYLINK}" --ponyc-job --args "${_stale}/version.lldargs"
        --mutate "${_stale}/missing.tmp" "${_stale}/mutate"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
set(_text "${_stdout}${_stderr}")
if(NOT _rc EQUAL 4 OR NOT _text MATCHES "couldn't write mutation info file")
    fail("stale: ponylink exited ${_rc}" "${_text}")
endif()
if(EXISTS "${_stale}/mutate")
    fail("stale: the earlier build's mutation info is still there" "${_text}")
endif()

# The compiler run directly, with a ponylink that rejects its options or is
# killed: the compiler says so and removes the link's temporary files itself.
# The compiler is copied next to the stub (as ponylink) with the runtime
# objects it links against, and finds the standard library through PONYPATH.
set(_cside "${_fixture}/cside")
file(MAKE_DIRECTORY "${_cside}/bin")
get_filename_component(_build_dir "${PONYC_COMPILER}" DIRECTORY)
file(GLOB _runtime "${_build_dir}/*.o" "${_build_dir}/libponyrt*")
file(COPY ${_runtime} DESTINATION "${_cside}/bin")
execute_process(COMMAND cp "${PONYC_COMPILER}" "${_cside}/bin/")
execute_process(COMMAND cp "${STUB}" "${_cside}/bin/ponylink")
get_filename_component(_packages "${CMAKE_CURRENT_LIST_DIR}/../packages"
    ABSOLUTE)
get_filename_component(_compiler_name "${PONYC_COMPILER}" NAME)
foreach(_case usage killed)
    if(_case STREQUAL "usage")
        set(_stub_env "STUB_PONYLINK_EXIT=1")
        set(_expected "internal error: ponylink rejected its options")
    else()
        set(_stub_env "STUB_PONYLINK_KILL=KILL")
        set(_expected "ponylink was killed by signal [0-9]+ \\([^)]*\\); the system may be out of memory")
    endif()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "PONYPATH=${_packages}" ${_stub_env}
            "${_cside}/bin/${_compiler_name}" --mutate -o "out-cside-${_case}" b
        WORKING_DIRECTORY "${_fixture}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    set(_text "${_stdout}${_stderr}")
    if(NOT _rc EQUAL 255 OR NOT _text MATCHES "${_expected}")
        fail("cside-${_case}: the compiler exited ${_rc}" "${_text}")
    endif()
    check_no_temporaries("cside-${_case}" "${_fixture}/out-cside-${_case}"
        "${_text}")
endforeach()

# The compiler given a handoff pipe runs ponylink itself for the first package;
# that ponylink must not have the pipe open, or the driver would wait for it
# to end before seeing the end of the job. b is built twice: a needs clang's
# headers, which the copied compiler can't find.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "PONYPATH=${_packages}"
        "STUB_LOG=${_fixture}/cside-cloexec.log"
        "${LAUNCHER}" --handoff-pipe
        -- "${_cside}/bin/${_compiler_name}" -o out-cside-cloexec b b
    WORKING_DIRECTORY "${_fixture}"
    OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
set(_text "${_stdout}${_stderr}")
set(_log "")
if(EXISTS "${_fixture}/cside-cloexec.log")
    file(READ "${_fixture}/cside-cloexec.log" _log)
endif()
if(NOT _text MATCHES "(^|\n)exited 0\n" OR
    NOT _text MATCHES "handoff fd ([0-9]+)\n")
    fail("cside-cloexec: the compiler failed" "${_text}\nStub log:\n${_log}")
endif()
set(_handoff_fd "${CMAKE_MATCH_1}")
if(NOT _log MATCHES "ponylink extra-fds=([0-9,]*)\n")
    fail("cside-cloexec: ponylink didn't run" "${_text}\nStub log:\n${_log}")
endif()
string(REPLACE "," ";" _fds "${CMAKE_MATCH_1}")
list(FIND _fds "${_handoff_fd}" _found)
if(NOT _found EQUAL -1)
    fail("cside-cloexec: ponylink has the handoff fd ${_handoff_fd} open"
        "${_text}\nStub log:\n${_log}")
endif()

# The compiler run directly with no ponylink next to it: it says so and
# removes the link's temporary files.
file(MAKE_DIRECTORY "${_cside}/nolink")
file(COPY ${_runtime} DESTINATION "${_cside}/nolink")
execute_process(COMMAND cp "${PONYC_COMPILER}" "${_cside}/nolink/")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "PONYPATH=${_packages}"
        "${_cside}/nolink/${_compiler_name}" --mutate -o out-cside-nolink b
    WORKING_DIRECTORY "${_fixture}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
set(_text "${_stdout}${_stderr}")
if(NOT _rc EQUAL 255 OR NOT _text MATCHES "ponylink not found at")
    fail("cside-nolink: the compiler exited ${_rc}" "${_text}")
endif()
check_no_temporaries("cside-nolink" "${_fixture}/out-cside-nolink" "${_text}")

# Linux and FreeBSD: ponyc killed by SIGKILL while the compiler runs takes the
# compiler with it. ponyc's stderr is a full FIFO that the script holds open
# and never reads, so the compiler stops at its first message ("Building ...")
# and is still running when ponyc is killed. The compiler is found as the
# driver's child. The test signals only the driver, its own child. A compiler
# still running when the test gives up gets SIGPIPE on its write once the
# script exits and nothing has the FIFO open for reading. On Linux the process table is read from
# /proc, since a minimal container may have no ps; FreeBSD's base system has
# ps.
if(LINUX OR CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD")
    file(WRITE "${_fixture}/pdeathsig.sh" [=[
ponyc="$1"
system="$2"
launcher="$3"

# Prints "<state> <ppid>" for a process, or nothing if there is none. The
# command name in /proc/<pid>/stat is in parentheses and may contain spaces,
# so the fields are taken from after its last ")".
proc_fields() {
  line=""
  { read -r line < "/proc/$1/stat"; } 2> /dev/null || return 0
  set -- ${line##*) }
  echo "$1 $2"
}

child_of() {
  if [ "$system" = "Linux" ]; then
    for stat in /proc/[0-9]*/stat; do
      pid=${stat#/proc/}
      pid=${pid%/stat}
      set -- $(proc_fields "$pid")
      if [ "${2:-}" = "$parent" ]; then
        echo "$pid"
        return 0
      fi
    done
  else
    ps -ax -o pid= -o ppid= | awk -v d="$parent" '$2 == d { print $1 }'
  fi
}

alive() {
  if [ "$system" = "Linux" ]; then
    set -- $(proc_fields "$compiler")
    state="${1:-}"
  else
    state=$(ps -o stat= -p "$compiler" 2> /dev/null)
  fi
  [ -n "$state" ] && [ "${state#Z}" = "$state" ]
}

rm -f stderr.fifo
mkfifo stderr.fifo || exit 1
exec 3<> stderr.fifo
if ! "$launcher" --fill-fifo stderr.fifo; then
  echo "unable to fill the FIFO"
  exit 1
fi
# Only the script holds the FIFO's read end: fd 3 is closed for ponyc.
"$ponyc" -o out-pdeathsig b > pdeathsig.log 2> stderr.fifo 3<&- &
driver=$!
parent=$driver
compiler=""
i=0
while [ -z "$compiler" ] && [ $i -lt 500 ]; do
  compiler=$(child_of)
  i=$((i + 1))
  sleep 0.01
done
if [ -z "$compiler" ]; then
  echo "no compiler started"
  exit 1
fi
# Killed earlier, ponyc could be gone before the compiler asks to die with it;
# the compiler waits at its first message, so this costs nothing.
sleep 0.5
kill -KILL "$driver"
wait "$driver"
status=$?
if [ $status -ne 137 ]; then
  echo "ponyc ended (status $status) before the test killed it"
  cat pdeathsig.log
  exit 1
fi
i=0
while alive && [ $i -lt 1000 ]; do
  i=$((i + 1))
  sleep 0.01
done
if alive; then
  echo "the compiler was still running at least ten seconds after ponyc was killed"
  exit 1
fi
echo "compiler gone"
]=])
    execute_process(COMMAND sh "${_fixture}/pdeathsig.sh" "${PONYC}"
            "${CMAKE_HOST_SYSTEM_NAME}" "${LAUNCHER}"
        WORKING_DIRECTORY "${_fixture}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    file(READ "${_fixture}/pdeathsig.log" _log)
    if(NOT _rc EQUAL 0 OR NOT _stdout MATCHES "compiler gone")
        fail("pdeathsig: the compiler outlived a killed ponyc"
            "${_stdout}${_stderr}\nCompiler output:\n${_log}")
    endif()
endif()

# macOS: ponylink rebuilds the dSYM bundle from the LTO objects, which it then
# removes. A bundle built without them holds no compile units.
if(APPLE)
    execute_process(
        COMMAND dwarfdump --debug-info "${_fixture}/out-single/a.dSYM"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _dwarf ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0 OR NOT _dwarf MATCHES "DW_TAG_compile_unit")
        fail("single: a.dSYM has no compile units" "${single_text}${_err}")
    endif()
endif()
