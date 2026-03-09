## Native Windows builds now use embedded LLD

Windows linking now uses the embedded LLD linker (COFF driver) instead of invoking the MSVC `link.exe` command. This is part of the ongoing work to eliminate external linker dependencies across all platforms.

If the embedded linker causes issues, use `--linker=<path-to-link.exe>` to fall back to the system linker.
