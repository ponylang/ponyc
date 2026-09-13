## Add `--lib-path` / `-L` flag for extra linker library search paths

The new `--lib-path` flag (short form `-L`) adds a library search directory that the linker checks before its auto-discovered paths. It can be specified multiple times.

Unlike `--path`, which adds directories to both Pony package resolution and the linker, `--lib-path` affects only the linker. This is useful when cross-compiling and the target-architecture libraries live outside the sysroot — for example, Debian multiarch packages installed to `/usr/lib/<triple>/`:

```
ponyc --triple riscv64-linux-gnu --lib-path /usr/lib/riscv64-linux-gnu mypackage
```
