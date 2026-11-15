## Fix `Path.is_abs` treating bare drive letters as absolute on Windows

`Path.is_abs("C:")` returned `true` on Windows, but `C:` is drive-relative — it refers to the current directory on drive C, not the root. `C:/` is the absolute form. `Path.abs("C:")` returned `"C:"` instead of joining with the working directory, and `Path.join(base, "C:")` returned `"C:"` instead of appending it as a relative component.

A drive letter is now absolute only when followed by a separator: `C:/` and `C:\` are absolute, `C:` and `C:foo` are not.
