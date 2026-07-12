## Fix Windows programs never finishing when stdin has ended

On Windows, a program that subscribed to stdin after stdin had ended could hang forever at 0% CPU. The new subscription never received a read, so the end of the input never reached the program, and the program never exited. A program run with its input redirected from `NUL` had a matching problem: the end of its input never arrived either, and it spun at 100% CPU instead of exiting.

A Windows program reading redirected stdin now reaches the end of its input the way it does on Linux and macOS.
