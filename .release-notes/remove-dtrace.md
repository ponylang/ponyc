## Remove DTrace and SystemTap support

DTrace and SystemTap USDT probes are no longer available. The `use=dtrace` build option has been removed.

The probes were incompatible with compiling the runtime as bitcode (`--runtimebc`), and they were only available on a subset of supported platforms (macOS, Linux, FreeBSD). The runtime's built-in tracing system (`runtime_info`, flight recorder) is not affected.
