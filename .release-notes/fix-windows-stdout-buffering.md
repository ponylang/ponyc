## Fix deferred stdout output on Windows

On Windows, stdout and stderr were left at the C runtime's default full buffering. Output from `env.out` could sit in the buffer and not appear until the program exited, especially when the program spent time in blocking FFI calls between prints. The same buffering was already configured on Unix (unbuffered for a terminal, line-buffered otherwise) but the Windows path was missing it.
