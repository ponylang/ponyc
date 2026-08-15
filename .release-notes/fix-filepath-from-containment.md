## Fix FilePath.from allowing paths outside the base directory

`FilePath.from` and `FilePath.join` are meant to keep the resulting path within the directory that the base `FilePath` grants access to. Two kinds of relative path slipped past that check and produced a `FilePath`, carrying the base's capabilities, that pointed outside the directory.

The first was a sibling whose name began with the base directory's name. Given a `FilePath` for `/srv/app`, `base.join("../app-backup/secret")` was accepted and named a file under `/srv/app-backup`, outside `/srv/app`. The second was a path containing an embedded NUL byte, which the check read in full while the operating system stopped at the NUL, so the two disagreed about which file was named. Because every `Directory` operation on a relative path routes through the same check, both escapes were reachable through `Directory` as well.

Both are now rejected with an error. A path is accepted only when it is the base path itself or lies beneath it at a directory boundary, and a path containing a NUL byte is refused.

Containment is still checked on the path text and does not resolve symlinks, so a symlink within the directory can still lead outside it.
