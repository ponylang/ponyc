# ctest wrapper: check ponyc (the driver, src/ponyc) on POSIX against a stub
# compiler and a stub ponylink (test/ponyc-driver/stub.c), started by
# test/ponyc-driver/launcher.c, which prints how the driver ended.
#
# Each case runs in its own directory holding a copy of the driver as ponyc,
# and copies of the stub under the compiler's name and as ponylink; the driver
# starts the compiler from its own directory, so a copy is needed rather than a
# symlink. The stubs append what they saw to stub.log in that directory.
#
# Args (passed with -D): PONYC (the driver), STUB, LAUNCHER,
# PONYC_COMPILER_NAME (the compiler's file name), WORKDIR (a scratch dir).

# The driver reports paths with symlinks and ".." resolved.
file(REAL_PATH "${WORKDIR}" _workdir)
set(_root "${_workdir}/ponyc-driver")
file(REMOVE_RECURSE "${_root}")

function(fail _name _text)
    message(FATAL_ERROR "${_name}\nOutput:\n${_text}")
endfunction()

# Set _out to _text with every character that has a meaning in a regex
# escaped, so a path can be matched literally.
function(regex_escape _out _text)
    string(REGEX REPLACE "([][+.*()^$?|\\{}])" "\\\\\\1" _escaped "${_text}")
    set(${_out} "${_escaped}" PARENT_SCOPE)
endfunction()

function(copy_program _from _to)
    execute_process(COMMAND cp "${_from}" "${_to}" RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "can't copy ${_from} to ${_to}")
    endif()
endfunction()

# run_case(<name> ENV <VAR=value>... LAUNCH <launcher option>... ARGS <arg>...
#   [NO_COMPILER] [SYMLINK | BARE_NAME | RELATIVE])
# Starts the case's driver by its path, or with SYMLINK through a symlink to it
# in another directory, with BARE_NAME as "ponyc" with its directory first on
# PATH, or with RELATIVE as "<name>/ponyc" from the parent directory. Sets
# <name>_out (the launcher's and driver's output), <name>_log (the stubs' log)
# and <name>_ponyc (the path or name the driver was started by) in the caller,
# and _dir to the case's directory, _dir_re to _dir escaped for a regex.
function(run_case _name)
    cmake_parse_arguments(_rc "NO_COMPILER;SYMLINK;BARE_NAME;RELATIVE" ""
        "ENV;LAUNCH;ARGS" ${ARGN})
    set(_dir "${_root}/${_name}")
    file(MAKE_DIRECTORY "${_dir}")
    copy_program("${PONYC}" "${_dir}/ponyc")
    copy_program("${STUB}" "${_dir}/ponylink")
    if(NOT _rc_NO_COMPILER)
        copy_program("${STUB}" "${_dir}/${PONYC_COMPILER_NAME}")
    endif()

    set(_ponyc "${_dir}/ponyc")
    set(_path_env "PATH=$ENV{PATH}")
    set(_cwd "${_dir}")
    if(_rc_SYMLINK)
        file(MAKE_DIRECTORY "${_root}/${_name}-link")
        set(_ponyc "${_root}/${_name}-link/ponyc")
        file(CREATE_LINK "${_dir}/ponyc" "${_ponyc}" SYMBOLIC)
    elseif(_rc_BARE_NAME)
        set(_ponyc "ponyc")
        set(_path_env "PATH=${_dir}:$ENV{PATH}")
    elseif(_rc_RELATIVE)
        set(_ponyc "${_name}/ponyc")
        set(_cwd "${_root}")
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
            "${_path_env}"
            "STUB_LOG=${_dir}/stub.log"
            "STUB_PONYLINK=${_dir}/ponylink"
            "STUB_READY=${_dir}/ready"
            ${_rc_ENV}
            "${LAUNCHER}" ${_rc_LAUNCH} -- "${_ponyc}" ${_rc_ARGS}
        WORKING_DIRECTORY "${_cwd}"
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    set(_log "")
    if(EXISTS "${_dir}/stub.log")
        file(READ "${_dir}/stub.log" _log)
    endif()
    set(${_name}_out "${_stdout}${_stderr}" PARENT_SCOPE)
    set(${_name}_log "${_log}" PARENT_SCOPE)
    set(${_name}_ponyc "${_ponyc}" PARENT_SCOPE)
    set(_dir "${_dir}" PARENT_SCOPE)
    regex_escape(_dir_re "${_dir}")
    set(_dir_re "${_dir_re}" PARENT_SCOPE)
endfunction()

# The driver ended as _expected ("exited N" or "signaled SIG").
function(check_end _name _expected)
    if(NOT "${${_name}_out}" MATCHES "(^|\n)${_expected}\n")
        fail("${_name}: the driver didn't end as '${_expected}'"
            "${${_name}_out}\nStub log:\n${${_name}_log}")
    endif()
endfunction()

function(check_log _name _regex _why)
    if(NOT "${${_name}_log}" MATCHES "${_regex}")
        fail("${_name}: ${_why}" "${${_name}_out}\nStub log:\n${${_name}_log}")
    endif()
endfunction()

function(check_not_log _name _regex _why)
    if("${${_name}_log}" MATCHES "${_regex}")
        fail("${_name}: ${_why}" "${${_name}_out}\nStub log:\n${${_name}_log}")
    endif()
endfunction()

function(check_out _name _regex _why)
    if(NOT "${${_name}_out}" MATCHES "${_regex}")
        fail("${_name}: ${_why}" "${${_name}_out}\nStub log:\n${${_name}_log}")
    endif()
endfunction()

function(check_not_out _name _regex _why)
    if("${${_name}_out}" MATCHES "${_regex}")
        fail("${_name}: ${_why}" "${${_name}_out}\nStub log:\n${${_name}_log}")
    endif()
endfunction()

function(check_no_ponylink _name)
    check_not_log(${_name} "ponylink argv" "ponylink ran")
endfunction()

# ponylink ran with --abandon right after --ponyc-job, then the rest of the
# valid job's arguments, and started with the four forwarded signals blocked.
function(check_abandoned _name)
    check_log(${_name}
        "ponylink argv\\[1\\]=--ponyc-job\nponylink argv\\[2\\]=--abandon\nponylink argv\\[3\\]=--args\nponylink argv\\[4\\]=stub.lldargs\n"
        "ponylink didn't run with --abandon right after --ponyc-job")
    check_not_log(${_name} "ponylink argv\\[5\\]" "ponylink got extra arguments")
    check_log(${_name} "ponylink blocked=INT,QUIT,TERM,HUP\n"
        "the abandoning ponylink didn't start with the forwarded signals blocked")
endfunction()

# SIGINT, SIGQUIT, SIGTERM, SIGHUP and SIGPIPE each have their default action
# in the role, and no signal the stub checks is blocked.
function(check_defaults _name _role)
    foreach(_sig INT QUIT TERM HUP PIPE)
        check_log(${_name} "${_role} ${_sig}=default"
            "${_role} didn't start with the default action for SIG${_sig}")
    endforeach()
    check_log(${_name} "${_role} blocked=\n"
        "${_role} started with signals blocked")
endfunction()

# A valid job: the compiler gets the handoff option first and the user's
# arguments unchanged, and ponylink runs the job.
run_case(valid ENV STUB_COMPILER_JOB=valid
    ARGS pkg "--flag=two words")
check_end(valid "exited 0")
check_log(valid "compiler argv\\[0\\]=${_dir_re}/ponyc\n"
    "the compiler's argv[0] isn't the driver's")
check_log(valid "compiler argv\\[1\\]=--link-handoff=[0-9]+\n"
    "the compiler's first argument isn't the handoff option")
check_log(valid "compiler argv\\[2\\]=pkg\ncompiler argv\\[3\\]=--flag=two words\n"
    "the compiler's other arguments changed")
check_not_log(valid "compiler argv\\[4\\]" "the compiler got extra arguments")
check_log(valid "ponylink argv\\[0\\]=${_dir_re}/ponylink\nponylink argv\\[1\\]=--ponyc-job\nponylink argv\\[2\\]=--args\nponylink argv\\[3\\]=stub.lldargs\n"
    "ponylink didn't get the job's arguments")
check_not_log(valid "ponylink argv\\[4\\]" "ponylink got extra arguments")
check_defaults(valid compiler)
check_defaults(valid ponylink)
foreach(_role compiler ponylink)
    check_log(valid "${_role} extra-fds=\n"
        "${_role} started with fds open besides 0 to 2 and the handoff pipe")
endforeach()

# ponylink starts only after the compiler has exited, even when the compiler
# goes on after closing the pipe.
run_case(order ENV STUB_COMPILER_JOB=valid STUB_COMPILER_LINGER_MS=300)
check_end(order "exited 0")
check_log(order "compiler exiting\n.*ponylink argv\\[0\\]="
    "ponylink started before the compiler exited")

# Nothing handed off: nothing to link.
run_case(nojob)
check_end(nojob "exited 0")
check_log(nojob "compiler argv" "the compiler didn't run")
check_no_ponylink(nojob)
check_not_out(nojob "ponyc: " "the driver printed a message")

# A job larger than a pipe holds: the driver reads all of it while the
# compiler is still writing, and ponylink gets every argument.
run_case(longjob ENV STUB_COMPILER_JOB=long)
check_end(longjob "exited 0")
check_log(longjob
    "ponylink argv\\[4003\\]=/nonexistent/stub-long-job-remove-1999\n"
    "ponylink didn't get the long job's last argument")
check_not_log(longjob "ponylink argv\\[4004\\]" "ponylink got extra arguments")

# A failed build: the compiler's status, and no link.
run_case(compilefail ENV STUB_COMPILER_EXIT=3)
check_end(compilefail "exited 3")
check_no_ponylink(compilefail)
check_not_out(compilefail "ponyc: " "the driver printed a message")

# The compiler fails or dies after handing off a job: the job's files are
# removed without linking.
run_case(failafterjob ENV STUB_COMPILER_JOB=valid STUB_COMPILER_EXIT=3)
check_end(failafterjob "exited 3")
check_abandoned(failafterjob)
check_not_out(failafterjob "ponyc: " "the driver printed a message")

run_case(killafterjob ENV STUB_COMPILER_JOB=valid STUB_COMPILER_KILL=USR1)
check_end(killafterjob "signaled USR1")
check_abandoned(killafterjob)
check_out(killafterjob
    "ponyc: ${PONYC_COMPILER_NAME} \\(pid [0-9]+\\) was killed by signal [0-9]+"
    "no message naming the compiler")
check_out(killafterjob
    "; to debug the compiler, run ${_dir_re}/${PONYC_COMPILER_NAME} with the same arguments\n"
    "no message naming the compiler to debug")

# The compiler killed by SIGKILL: the message says the system may be out of
# memory.
run_case(compilerkilled ENV STUB_COMPILER_KILL=KILL)
check_end(compilerkilled "signaled KILL")
check_out(compilerkilled
    "killed by signal [0-9]+ \\([^)]*\\); the system may be out of memory"
    "no out-of-memory hint")
check_no_ponylink(compilerkilled)

# The compiler dies of SIGPIPE, as when the reader of its output has gone
# away: the driver dies of it too, without printing an error.
run_case(compilerpipe ENV STUB_COMPILER_KILL=PIPE)
check_end(compilerpipe "signaled PIPE")
check_not_out(compilerpipe "Error:" "printed an error")
check_no_ponylink(compilerpipe)

# A malformed job: an internal error, and no ponylink. Each kind differs from
# a valid job in one way (stub.c).
foreach(_kind unterminated truncated wrong-second empty-first relative-first
    short)
    run_case(${_kind} ENV STUB_COMPILER_JOB=${_kind})
    check_end(${_kind} "exited 255")
    check_no_ponylink(${_kind})
    check_out(${_kind} "malformed link" "no malformed link message")
endforeach()

# ponylink fails with PONYLINK_EXIT_FAILED, after printing why: the driver
# removes the job's files, prints nothing of its own, and exits 255.
run_case(linkfail ENV STUB_COMPILER_JOB=valid STUB_PONYLINK_EXIT=255)
check_end(linkfail "exited 255")
check_abandoned(linkfail)
check_not_out(linkfail "ponyc: " "the driver printed a message")

# ponylink exits with a status it doesn't define: the driver describes it and
# exits 255.
run_case(linkodd ENV STUB_COMPILER_JOB=valid STUB_PONYLINK_EXIT=7)
check_end(linkodd "exited 255")
check_abandoned(linkodd)
check_out(linkodd "ponyc: ponylink failed \\(exit 7\\)"
    "no message describing ponylink's exit")

# ponylink rejects its options: the driver says so and exits 255, without an
# --abandon run, which would reject them too (the job's files stay).
run_case(linkusage ENV STUB_COMPILER_JOB=valid STUB_PONYLINK_EXIT=1)
check_end(linkusage "exited 255")
check_not_log(linkusage "ponylink argv\\[2\\]=--abandon"
    "the driver ran ponylink --abandon")
check_out(linkusage
    "ponyc: internal error: ponylink at ${_dir_re}/ponylink rejected its options"
    "no message about ponylink's options")

# ponylink can't be started: the driver says so and fails.
run_case(nolink ENV STUB_COMPILER_JOB=valid "STUB_PONYLINK=${_root}/no-such-ponylink")
check_end(nolink "exited 255")
regex_escape(_root_re "${_root}")
check_out(nolink "ponyc: unable to start ponylink at ${_root_re}/no-such-ponylink: "
    "no message about starting ponylink")

# ponylink dies of a signal: the driver says so, removes the job's files and
# dies of it too.
run_case(linkkilled ENV STUB_COMPILER_JOB=valid STUB_PONYLINK_KILL=USR1)
check_end(linkkilled "signaled USR1")
check_abandoned(linkkilled)
check_out(linkkilled "ponyc: ponylink was killed by signal [0-9]+"
    "no message about ponylink")

# ponylink dies of SIGPIPE: no error printed, and the driver dies of it too.
run_case(linkpipe ENV STUB_COMPILER_JOB=valid STUB_PONYLINK_KILL=PIPE)
check_end(linkpipe "signaled PIPE")
check_abandoned(linkpipe)
check_not_out(linkpipe "Error:" "printed an error")

# Started through a symlink in another directory, and by bare name with its
# directory on PATH: the driver finds the compiler next to its real path and
# passes on argv[0] as given.
run_case(viasymlink ENV STUB_COMPILER_JOB=valid SYMLINK)
check_end(viasymlink "exited 0")
regex_escape(_link_re "${viasymlink_ponyc}")
check_log(viasymlink "compiler argv\\[0\\]=${_link_re}\n"
    "the compiler's argv[0] isn't the symlink's path")
check_log(viasymlink "ponylink argv" "ponylink didn't run")

run_case(barename ENV STUB_COMPILER_JOB=valid BARE_NAME)
check_end(barename "exited 0")
check_log(barename "compiler argv\\[0\\]=ponyc\n"
    "the compiler's argv[0] isn't the bare name")
check_log(barename "ponylink argv" "ponylink didn't run")

# Started by a relative path that contains a slash.
run_case(relpath ENV STUB_COMPILER_JOB=valid RELATIVE)
check_end(relpath "exited 0")
check_log(relpath "compiler argv\\[0\\]=relpath/ponyc\n"
    "the compiler's argv[0] isn't the relative path")
check_log(relpath "ponylink argv" "ponylink didn't run")

# A terminating signal sent to the driver reaches the compiler, and the
# driver then dies of it.
foreach(_sig TERM INT HUP QUIT)
    run_case(signal-${_sig} ENV STUB_COMPILER_WAIT=1
        LAUNCH --wait-file ready --signal ${_sig})
    check_end(signal-${_sig} "signaled ${_sig}")
    check_log(signal-${_sig} "compiler received ${_sig}\n"
        "SIG${_sig} didn't reach the compiler")
    check_no_ponylink(signal-${_sig})
    check_not_out(signal-${_sig} "was killed by signal"
        "a message for a signal sent to the driver")
endforeach()

# A terminating signal sent to the driver while ponylink runs the handed-off
# link reaches ponylink, and the driver then dies of it.
foreach(_sig TERM INT HUP QUIT)
    run_case(signal-link-${_sig} ENV STUB_COMPILER_JOB=valid STUB_PONYLINK_WAIT=1
        LAUNCH --wait-file ready --signal ${_sig})
    check_end(signal-link-${_sig} "signaled ${_sig}")
    check_log(signal-link-${_sig} "ponylink received ${_sig}\n"
        "SIG${_sig} didn't reach ponylink")
    check_abandoned(signal-link-${_sig})
    check_not_out(signal-link-${_sig} "was killed by signal"
        "a message for a signal sent to the driver")
endforeach()

# Started with SIGUSR1 blocked: the compiler and ponylink start with the mask
# the driver started with, not an empty one.
run_case(blockedmask ENV STUB_COMPILER_JOB=valid LAUNCH --block USR1)
check_end(blockedmask "exited 0")
foreach(_role compiler ponylink)
    check_log(blockedmask "${_role} blocked=USR1\n"
        "${_role} didn't start with the driver's signal mask")
endforeach()

# A terminating signal sent to the driver after the compiler handed off its
# job: the compiler dies of it, the job's files are removed without linking,
# and the driver dies of the signal.
run_case(signalafterjob ENV STUB_COMPILER_JOB=valid STUB_COMPILER_WAIT=1
    LAUNCH --wait-file ready --signal TERM)
check_end(signalafterjob "signaled TERM")
check_log(signalafterjob "compiler received TERM\n"
    "SIGTERM didn't reach the compiler")
check_abandoned(signalafterjob)
check_not_out(signalafterjob "was killed by signal"
    "a message for a signal sent to the driver")

# The driver's signal wins over the one the compiler dies of.
run_case(signalwins ENV STUB_COMPILER_WAIT=1 STUB_COMPILER_ON_TERM=USR1
    LAUNCH --wait-file ready --signal TERM)
check_end(signalwins "signaled TERM")
check_log(signalwins "compiler received TERM\n"
    "SIGTERM didn't reach the compiler")

# A signal that arrives after the job, before ponylink starts: no link, the
# job's files are removed, and the driver dies of the signal.
run_case(signalbeforelink
    ENV STUB_COMPILER_JOB=valid STUB_COMPILER_SIGNAL_PARENT=TERM)
check_end(signalbeforelink "signaled TERM")
check_abandoned(signalbeforelink)

# Started with SIGHUP ignored (nohup): it stays ignored in the compiler and
# ponylink; the rest keep their default actions.
run_case(nohup ENV STUB_COMPILER_JOB=valid LAUNCH --ignore HUP)
check_end(nohup "exited 0")
foreach(_role compiler ponylink)
    check_log(nohup "${_role} HUP=ignore" "SIGHUP isn't ignored in ${_role}")
    foreach(_sig INT QUIT TERM PIPE)
        check_log(nohup "${_role} ${_sig}=default"
            "${_role} didn't start with the default action for SIG${_sig}")
    endforeach()
endforeach()

# Started with stdin, stdout, or stdout and stderr closed: the children start
# with fds 0 to 2 open, the handoff pipe is fd 3 or above (the stub compiler
# exits 99 otherwise), and the link runs.
foreach(_case closed0 closed1 closed12)
    if(_case STREQUAL "closed0")
        run_case(${_case} ENV STUB_COMPILER_JOB=valid LAUNCH --close 0)
    elseif(_case STREQUAL "closed1")
        run_case(${_case} ENV STUB_COMPILER_JOB=valid LAUNCH --close 1)
    else()
        run_case(${_case} ENV STUB_COMPILER_JOB=valid LAUNCH --close 1 --close 2)
    endif()
    check_end(${_case} "exited 0")
    check_log(${_case} "ponylink argv" "ponylink didn't run")
    foreach(_role compiler ponylink)
        check_log(${_case} "${_role} fd0=open\n${_role} fd1=open\n${_role} fd2=open\n"
            "${_role} started with a standard fd closed")
    endforeach()
endforeach()

# Started with SIGCHLD ignored: the driver still waits for its children and
# the link runs.
run_case(nochld ENV STUB_COMPILER_JOB=valid LAUNCH --ignore CHLD)
check_end(nochld "exited 0")
check_log(nochld "ponylink argv" "ponylink didn't run")

# No compiler next to the driver.
run_case(nocompiler NO_COMPILER)
check_end(nocompiler "exited 255")
check_out(nocompiler
    "ponyc: unable to start the ponyc compiler at ${_dir_re}/${PONYC_COMPILER_NAME}"
    "no message naming the compiler")
