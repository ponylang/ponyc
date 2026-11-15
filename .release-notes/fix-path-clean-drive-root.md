## Fix Path.clean stripping trailing separator from volume roots

`Path.clean` stripped the trailing separator from Windows drive roots and network share roots. `clean("C:/")` produced `"C:"` and `clean("//host/share/")` produced `"//host/share"`, even though both are root directories whose trailing separator should be preserved.

`Path.split`, `Path.dir`, and `Path.join` all call `clean` internally, so the same stripping affected round-trips through those functions. `Path.join(Path.split("C:/a"))` produced `"C:a"` instead of `"C:/a"` because `split` returned `("C:", "a")` as the head.

Root directories now keep their trailing separator, matching the documented behavior: "The result will have no trailing slash unless it is a root directory."
