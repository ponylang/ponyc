## Fix Path.abs and Path.rel not resolving drive-relative paths on Windows

`Path.abs("C:foo")` on Windows joined `C:foo` literally onto the process CWD, producing a path like `C:/Users/.../C:foo` instead of resolving `foo` relative to the current directory on drive C. Each drive on Windows can have its own current directory; `C:foo` means "foo relative to the current directory on drive C," which may differ from the process CWD when the process is on a different drive.

`Path.rel` had the same problem with drive-relative arguments. Both are now fixed.
