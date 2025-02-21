## Fix being unable to compile programs that use runtime info on Windows

We accidentally weren't properly exporting the Pony runtime methods needed by the "runtime_info" package so that they could be linked on Windows.
