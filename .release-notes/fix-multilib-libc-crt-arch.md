## Reject wrong-architecture libc startup objects on multilib hosts

Compiling a Pony program on a multilib Linux host could fail with a confusing arch-mismatch error from the embedded linker. For example, on an x86_64 Fedora system with `glibc-devel.i686` installed, ponyc would pick up the 32-bit `/usr/lib/crt1.o` and the link would fail with:

```
ld.lld: error: /usr/lib/crt1.o is incompatible with elf_x86_64
```

ponyc now validates the architecture of each candidate `crt1.o` and skips ones that don't match the target. If no matching `crt1.o` is found, the error message names the target architecture instead of falling through to the linker's lower-level error.
