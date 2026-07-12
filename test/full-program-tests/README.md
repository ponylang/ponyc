# Full-program tests

Each directory here is one Pony program. The runner compiles it with the ponyc
that was just built, runs it, and compares its exit code with what the test
expects. A directory with no `.pony` source in it is not a test.

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

## The debugger

A test that says anything about its stdin runs without the debugger, even when
one is set. lldb gives the program it launches a terminal for stdin, so such a
program would not be handed the pipe, would not read what the tester wrote, and
would not reach the end of its input. The cost is no backtrace if one of these
tests crashes.
