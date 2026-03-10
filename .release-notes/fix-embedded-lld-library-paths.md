## Fix embedded linker missing standard system library search paths

The embedded LLD linker did not search standard system library directories beyond where libc CRT objects are installed. Libraries in `/usr/local/lib` (common for libraries built from source, such as OpenSSL), `/usr/local/lib64` (OpenSSL on x86_64 Linux), or `/lib` (on systems that haven't done the /usr merge, such as Alpine Linux 3.20 and earlier) could not be found, causing "unable to find library" link errors.

The embedded linker now includes `/lib`, `/usr/local/lib`, `/lib64`, `/usr/lib64`, and `/usr/local/lib64` as fallback search paths on Linux, and `/opt/homebrew/lib` and `/usr/local/lib` on macOS.
