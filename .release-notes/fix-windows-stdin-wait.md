## Fix Windows programs stalling while reading stdin from a pipe

On Windows, a read of stdin from a pipe did not return until the program on the other end of the pipe wrote a byte or closed the pipe. With one scheduler thread, nothing else in the program ran in the meantime: no timer fired and no other actor ran. With the default number of scheduler threads, the rest of the program kept running, but `env.input.dispose()` had no effect until the other end wrote a byte or closed the pipe, so the program kept reading stdin and could not exit.

The rest of a Windows program now runs while it waits for input on stdin, and `env.input.dispose()` takes effect when you call it, as it already did on Linux and macOS. Console input, stdin redirected from a file, and stdin from NUL were never affected.
