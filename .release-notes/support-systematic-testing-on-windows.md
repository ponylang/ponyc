## Support building with systematic testing on Windows

The runtime's systematic testing mode can now be built and run on Windows. Previously it was only available on Linux and macOS: the Windows build script had no way to turn on runtime use options, and the systematic testing runtime had never been compiled for Windows.

Enable it by passing `-Use systematic_testing` to `make.ps1 configure`. Unlike the Linux and macOS builds, Windows does not pair it with `scheduler_scaling_pthreads` — it is enabled on its own, because Windows scales the scheduler with native primitives rather than pthreads.

```
.\make.ps1 configure -Config Debug -Use systematic_testing
.\make.ps1 build -Config Debug
```

A program built this way replays a single scheduler interleaving from a fixed `--ponysystematictestingseed`, the same as on the other platforms.
