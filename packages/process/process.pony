"""
# Process package

The Process package provides support for handling Unix style processes.
For each external process that you want to handle, you need to create a
`ProcessMonitor` and a corresponding `ProcessNotify` object. Each
ProcessMonitor runs as it own actor and upon receiving data will call its
corresponding `ProcessNotify`'s method.

## Example program

The following program will spawn an external program and write to it's
STDIN. Output received on STDOUT of the child process is forwarded to the
ProcessNotify client and printed.

```pony
use "backpressure"
use "process"
use "files"

actor Main
  new create(env: Env) =>
    // create a notifier
    let client = ProcessClient(env)
    let notifier: ProcessNotify iso = consume client
    // define the binary to run
    let path = FilePath(FileAuth(env.root), "/bin/cat")
    // define the arguments; first arg is always the binary name
    let args: Array[String] val = ["cat"]
    // define the environment variable for the execution
    let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]
    // create a ProcessMonitor and spawn the child process
    let sp_auth = StartProcessAuth(env.root)
    let bp_auth = ApplyReleaseBackpressureAuth(env.root)
    let pm: ProcessMonitor = ProcessMonitor(sp_auth, bp_auth, consume notifier,
    path, args, vars)
    // write to STDIN of the child process
    pm.write("one, two, three")
    pm.done_writing() // closing stdin allows cat to terminate

// define a client that implements the ProcessNotify interface
class ProcessClient is ProcessNotify
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    let out = String.from_array(consume data)
    _env.out.print("STDOUT: " + out)

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    let err = String.from_array(consume data)
    _env.out.print("STDERR: " + err)

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    _env.out.print(err.string())

  fun ref dispose(process: ProcessMonitor ref, child_exit_status: ProcessExitStatus) =>
    match child_exit_status
    | let exited: Exited =>
      _env.out.print("Child exit code: " + exited.exit_code().string())
    | let signaled: Signaled =>
      _env.out.print("Child terminated by signal: " + signaled.signal().string())
    end
```

## Process portability

The ProcessMonitor supports spawning processes on Linux, FreeBSD, OSX and
Windows.

## Shutting down ProcessMonitor and external process

When a process is spawned using ProcessMonitor, and it is not necessary to
communicate to it any further using `stdin` and `stdout` or `stderr`, calling
[done_writing()](process-ProcessMonitor.md#done_writing) will close stdin to
the child process. Processes expecting input will be notified of an `EOF` on
their `stdin` and can terminate.

If a running program needs to be canceled and the
[ProcessMonitor](process-ProcessMonitor.md) should be shut down, calling
[dispose](process-ProcessMonitor.md#dispose) will terminate the child process
and clean up all resources.

Once the child process is detected to be closed, the process exit status is
retrieved and [ProcessNotify.dispose](process-ProcessNotify.md#dispose) is
called.

The process exit status can be either an instance of
[Exited](process-Exited.md) containing the process exit code in case the
program exited on its own, or (only on posix systems like linux, osx or bsd) an
instance of [Signaled](process-Signaled.md) containing the signal number that
terminated the process.
"""
