## Don't allow --path to override standard library

Previously, directories passed via `--path` on the ponyc command line were searched before the standard library when resolving package names. This meant a `--path` directory could contain a subdirectory that shadows a stdlib package. For example, `--path /usr/lib` would cause `/usr/lib/debug/` to shadow the stdlib `debug` package, breaking any code that depends on it.

The standard library is now always searched first, before any `--path` entries. This is the same fix that was applied to `PONYPATH` in [#3779](https://github.com/ponylang/ponyc/issues/3779).

If you were relying on `--path` to override a standard library package, that will no longer work.
