## Make runtime bitcode the default

On clang-based platforms (Linux, macOS), ponyc now optimises across the boundary between your code and the runtime by default. Binaries are smaller and faster with no flag required. On Windows/MSVC, where bitcode is not available, linking behaviour is unchanged.

The `--runtimebc` CLI flag has been removed. If your build scripts pass `--runtimebc`, remove it — the behaviour is now automatic.

On macOS, shared libraries that call PONY_API runtime functions (`pony_exitcode`, `pony_alloc`, etc.) must no longer link `libponyrt`. The runtime now lives in the executable, and linking `libponyrt` into a shared library creates a second copy of runtime state. Instead, build the shared library with `-undefined dynamic_lookup` so its runtime calls resolve from the executable at load time. On Linux and FreeBSD no change is needed — ELF's flat symbol namespace resolves from the executable by default.
