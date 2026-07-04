## Fix systematic testing occasionally hanging on Windows

On Windows, a program built with `use=systematic_testing` could occasionally hang instead of finishing. The run would stop making progress and never exit, most often as it was shutting down, and it only happened with more than one scheduler thread. Running the same program again would usually complete normally. This has been fixed.
