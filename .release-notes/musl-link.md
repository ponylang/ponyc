## Fix linking on Linux arm64 when using musl libc

On arm64-based Linux systems using musl libc, program linking could fail due to missing libgcc symbols. We've now added linking against libgcc on all arm-based Linux systems to resolve this issue.
