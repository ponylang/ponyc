## Fix undefined memory read for overlong paths on Windows

On Windows, `FilePath` creation and `Path.abs` read undefined memory when the resolved path exceeded 260 characters, instead of returning an error. Paths that long are rare on Windows without `\\?\` prefixes, but the runtime now fails cleanly instead of returning garbage.
