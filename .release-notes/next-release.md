## Fix Windows programs never finishing when stdin has ended

On Windows, a program that subscribed to stdin after stdin had ended could hang forever at 0% CPU. The new subscription never received a read, so the end of the input never reached the program, and the program never exited. A program run with its input redirected from `NUL` had a matching problem: the end of its input never arrived either, and it spun at 100% CPU instead of exiting.

A Windows program reading redirected stdin now reaches the end of its input the way it does on Linux and macOS.

## Fix Windows programs stalling while reading stdin from a pipe

On Windows, a read of stdin from a pipe did not return until the program on the other end of the pipe wrote a byte or closed the pipe. With one scheduler thread, nothing else in the program ran in the meantime: no timer fired and no other actor ran. With the default number of scheduler threads, the rest of the program kept running, but `env.input.dispose()` had no effect until the other end wrote a byte or closed the pipe, so the program kept reading stdin and could not exit.

The rest of a Windows program now runs while it waits for input on stdin, and `env.input.dispose()` takes effect when you call it, as it already did on Linux and macOS. Console input, stdin redirected from a file, and stdin from NUL were never affected.

## Fix a new Windows stdin notifier getting a read posted for the one it replaced

On Windows, a program that disposed its stdin notifier and installed a new one could have the new notifier sent a read that was posted for the notifier it replaced.

Reading stdin through `env.input` and the standard library's `Stdin` actor, nothing was lost, duplicated, or reordered, so most programs saw no difference. A program that subscribed to stdin readiness through the asio event API directly could be sent a read posted for a notifier it had already replaced. A new notifier is now sent only the reads posted for it.

