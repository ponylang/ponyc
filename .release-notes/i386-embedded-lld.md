## Add i386 (32-bit x86) support to the embedded LLD linker

The embedded LLD linker can now link Pony programs targeting 32-bit x86 Linux (i386/i686). Previously, compiling for an i386 target failed at link time with "unsupported architecture for embedded LLD." The embedded linker already supported 32-bit ARM; this extends the same support to 32-bit x86, covering both musl and glibc systems.
