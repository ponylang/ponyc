## Fix in-place rebuild of the standalone compiler library on FreeBSD and OpenBSD

On FreeBSD and OpenBSD, building ponyc a second time in an existing build
directory (reconfiguring in place rather than starting from a clean tree)
failed while generating `libponyc-standalone.a`:

```
cp: libcpp.a: Permission denied
```

The build copied the system C++ runtime (`libc++.a`, which is read-only) into
the build directory. Because the copy inherited the source file's read-only
permissions, a later build could not overwrite it. The copy now forces
replacement, so repeated in-place builds succeed.
