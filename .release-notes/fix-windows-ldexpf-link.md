## Fix `F32.ldexp` linking on Windows

Any program that called `F32.ldexp` failed to build on Windows:

```console
unable to link: lld-link: error: undefined symbol: ldexpf
```

This has been fixed.
