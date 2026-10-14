## Use forward slash as the path separator on all platforms

All paths in the `files` package now use `/` as the separator on every platform, including Windows. Paths from `Path.cwd()` and `Path.canonical()` are `/`-separated, `Path.sep()` returns `"/"` unconditionally, and `Path.clean()` converts `\` to `/` on Windows.

`Path.is_sep()` still accepts `\` on Windows for parsing external input. Use `Path.to_os()` when a backslash path is needed for Windows shell commands.

`Path.normcase()` now only lowercases the path on case-insensitive platforms. It no longer replaces `/` with `\` on Windows.

Before:

```pony
if path == "C:\\Users\\me\\project" then
  // ...
end
```

After:

```pony
if path == "C:/Users/me/project" then
  // ...
end
```

Code that passes paths to Windows shell commands should use `Path.to_os()`:

```pony
let os_path = Path.to_os(path)
```

## Remove Path.from_slash and Path.to_slash

`Path.from_slash()` and `Path.to_slash()` have been removed. All paths use `/` as the separator on every platform, so these functions served no purpose. Remove any calls to them.
