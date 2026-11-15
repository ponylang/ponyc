## Fix Path.clean treating drive-relative paths as rooted on Windows

`Path.clean` treated any path with a volume prefix as rooted, including drive-relative paths like `C:foo`. On Windows, `C:foo` is relative to the current directory on drive C, not to the root of C. Because `clean` treated it as rooted, `..` components were silently discarded: `clean("C:../foo")` returned `"C:foo"` instead of the correct `"C:../foo"`, and `clean("C:foo/../bar")` returned `"C:foo/bar"` instead of `"C:bar"`. The bug propagated through `join`, `abs`, `rel`, `split`, and `dir`, which all call `clean`.
