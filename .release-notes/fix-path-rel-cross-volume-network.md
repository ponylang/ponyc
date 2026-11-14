## Fix `Path.rel` producing wrong results for cross-volume `//` paths on non-Windows

`Path.rel` silently returned a wrong relative path when called with two `//` network paths on different volumes (e.g., `Path.rel("//host1/share1/a", "//host2/share2/b")`). Joining the result back produced a path on the first volume, not the second. The call now correctly errors when the two paths have different volumes, matching the existing behavior on Windows.
