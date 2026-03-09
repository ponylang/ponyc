## Native macOS builds now use embedded LLD

macOS linking now uses the embedded LLD linker (Mach-O driver) instead of invoking the system `ld` command. This is part of the ongoing work to eliminate external linker dependencies across all platforms.

If the embedded linker causes issues, use `--linker=ld` to fall back to the system linker.
