## Fix crash when disposing stdin input on Windows

On Windows, a program that stopped reading from stdin while input was still arriving could crash or corrupt memory. This happened when the stdin notifier was disposed — for example via `env.input.dispose()` — as data continued to stream in. It is now fixed.
