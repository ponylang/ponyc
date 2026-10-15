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
## Fix iftype specialization on method-level type parameters

When a method had its own type parameter with an iftype specialization guard, the compiler selected the specialized body unconditionally — even when the concrete type argument did not satisfy the guard. Class-level type parameter guards were not affected.

```pony
primitive Foo
  fun bar[S: ReadSeq[U8] box](seq: S): U16 ? =>
    (seq(0)?.u16() << 8) or seq(1)?.u16()

  fun bar[S: ReadSeq[U8] box](seq: S): U16
    iftype S <: Array[U8] box
  ? =>
    seq.read_u16(0)?
```

Calling `Foo.bar[WrappedSeq](seq)?` would fail to compile with "can't find a compatible 'read_u16' method on type 'WrappedSeq ref'" because the compiler chose the `Array[U8]`-specialized body instead of the default.

