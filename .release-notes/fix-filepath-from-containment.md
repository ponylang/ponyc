## Fix FilePath.from allowing paths outside the base directory

`FilePath.from` and `FilePath.join` are meant to keep the resulting path within the directory that the base `FilePath` grants access to. Two kinds of relative path were accepted despite pointing outside it, producing a `FilePath` that carried the base's capabilities.

The first was a sibling whose name began with the base directory's name. Given a `FilePath` for `/srv/app`, `base.join("../app-backup/secret")` was accepted and named a file under `/srv/app-backup`, outside `/srv/app`. The second was a path containing an embedded NUL byte: it was accepted, and the operating system, which stops reading a path at the first NUL, then acted on a shorter path naming a different file. `Directory` operations that take a relative path were affected in the same way.

Both are now rejected with an error. A path is accepted only when it is the base path itself or a path below it, and never when it contains a NUL byte.

Containment is still checked on the path text and does not resolve symlinks, so a symlink within the directory can still lead outside it.
