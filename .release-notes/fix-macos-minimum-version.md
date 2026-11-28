## Fix ponyc requiring the macOS version it was built on

On macOS, the `ponyc` binary was built for the macOS version of the machine that built it, instead of macOS 13 like the rest of Pony's macOS binaries. Recent releases, including 0.78.2, require macOS 26 on Apple silicon and macOS 15 on Intel. `ponyc` is now built for macOS 13.
