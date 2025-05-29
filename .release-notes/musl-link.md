## Fix linking on Linux arm64 when using musl libc

When linking programs on arm64 based Linux systems that use musl libc, linking could fail. Some symbols weren't being linked as they would with glibc. Linking again libgcc is required. We've now added linking again libgcc on all arm based Linux systems.
