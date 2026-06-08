## Use embedded LLD for FreeBSD use=dtrace builds

When ponyc is built with `use=dtrace` on FreeBSD, it previously linked the programs it compiles through the system C compiler driver instead of the embedded LLD linker that other FreeBSD builds use. It now links those programs with embedded LLD as well, so a `use=dtrace` ponyc no longer needs an external C compiler driver to link the programs it builds, and their dtrace probes register and fire exactly as before.
