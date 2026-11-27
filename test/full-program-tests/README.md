# Full-program tests

Each directory here is one Pony program. The runner compiles it with the ponyc
that was just built, runs it, and checks it against what the test expects: its
exit code, and its stdout when the test gives one. A test that must fail to
compile is not run; its compile errors are checked instead. A directory with no
`.pony` source in it is not a test.

The runner finds tests by scanning this directory, so a new test needs no
registration. It runs each program with that program's own directory as the
working directory.

## What a test directory can hold

Besides the Pony source, a test directory can hold any of these files. All of
them are optional.

### `expected-exit-code.txt`

The exit code the program is expected to exit with. A test with no
`expected-exit-code.txt` is expected to exit with 0.

### `stdin.txt`

What the tester writes to the program's stdin. Without one, the program's stdin
is closed as soon as it starts.

An empty `stdin.txt` is not the same as no `stdin.txt`: it asks for a stdin that
is open and then closed with nothing in it.

The bytes are written as they are. Give a test that reads its input a `stdin.txt`
with no line endings in it, or say how it should read the ones it has.

### `stdin-delay-seconds.txt`

A number of seconds to hold the program's stdin open, writing nothing to it,
before writing `stdin.txt` (if there is one) and closing it. Without this file,
the tester writes and closes at once.

This is how a test gets a stdin that is open with no input on it. The delay is
real time: it is added to how long the test takes.

The tester cannot hold a program's stdin open for the whole of its life. It gets
the program's exit code by way of a `ProcessMonitor`, which does not reap a child
until it has closed the child's stdin, so a stdin that never closed would mean a
test that never reported.

### `program-args.txt`

Arguments to pass to the program, one to a line. Blank lines are skipped. This is
how a test passes runtime options, such as `--ponymaxthreads=1`.

### `expected-stdout.txt`

What the program must print. The tester compares it with everything the program
wrote to stdout, exactly, after checking the exit code. Line endings written as
CRLF in either one are read as LF.

### `expected-compile-errors.txt`

The program must fail to compile, with these errors. A test with this file is not
run, and it can't also have `expected-stdout.txt` or `expected-exit-code.txt`; a
test that has both is reported as broken.

Each error's headline is one line, `path:line:col: message`, with the path
relative to the test's directory and written with `/`. The headlines must match
the errors ponyc reports, in the same order and the same number. Lines under a
headline that start with four spaces are its notes, in the same form; they must
appear among that error's notes, in the same order, but the error may have notes
that aren't listed. Two kinds of note must be listed when the error has them: the
note that names the type a method was inherited by ("in method 'f' inherited by
C from T"), and the note that says how a private type reached the code ("... and
reaches this code through ..."). Leaving one out fails the test.

## Tree checks

The runner's `--checktree` option compiles every test with ponyc's
`--checktree`, which checks the compiler's AST after each pass. The ctest entries
for these tests pass it when the environment sets `PONY_CHECKTREE=1`.

## The debugger

A test that says anything about its stdin, or has an `expected-stdout.txt`, runs
without the debugger, even when one is set. lldb gives the program it launches a
terminal for stdin, so such a program would not be handed the pipe, would not
read what the tester wrote, and would not reach the end of its input. And under
lldb, what the tester captures from stdout is lldb's transcript, not the
program's output. The cost is no backtrace if one of these tests crashes.
