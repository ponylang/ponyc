## Fix use=dtrace builds on macOS

Building ponyc with `use=dtrace` failed on macOS because the build tried to run `dtrace -G`, a flag that macOS dtrace has never supported. macOS's linker resolves DTrace probe symbols natively, so the `-G` step was never needed — only the `dtrace -h` header generation step is required.

`use=dtrace` now builds and links correctly on macOS. Programs compiled by a dtrace-enabled ponyc expose their `pony` provider probes to DTrace. Actually tracing a running program requires System Integrity Protection (SIP) to permit DTrace; see the examples/dtrace README for details.
