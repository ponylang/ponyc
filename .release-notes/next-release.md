## Fix `use=dtrace` builds on FreeBSD

Building ponyc with `use=dtrace` failed on FreeBSD: the runtime build aborted with `dtrace: failed to link script ... No probe sites found for declared provider`, and even past that, programs compiled by a dtrace-enabled ponyc could not be linked.

`use=dtrace` now builds and links correctly on FreeBSD, and dynamically-linked programs expose their `pony` provider probes to DTrace.

`--static` programs build and run but do not expose their probes: FreeBSD registers DTrace USDT probes through the runtime linker, which statically-linked programs don't use.

## Use embedded LLD for native Linux sanitizer builds

When ponyc is built with sanitizers (such as `address_sanitizer` or `undefined_behavior_sanitizer`) on Linux, it now links the programs it compiles with its built-in LLD linker, the same as every other build, instead of falling back to your system C compiler to perform the link. Sanitizer-enabled native Linux compilation no longer depends on having an external compiler driver present and usable as a linker.

