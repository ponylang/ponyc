## Port Pony to Haiku operating system

Initial port to Haiku OS.

Some workarounds had to be made that should be fixed at some point in future:
- In `packages/net/tcp_connection.pony` there's a workaround for
  a problem with sockets not being closed at all on Haiku;
- Pony on Haiku will incorrectly parse `NaN(123)` cases:
  In `packages/builtin_test/_test.pony` there's a workaround which
  passes the test if implementation is still broken on Haiku.
  When it fails, it will mean Haiku's part was fixed and workaround
  can be removed. This problem is tracked by Haiku at:
  https://dev.haiku-os.org/ticket/20092

Things not tried:
- `LTO`,
- building apps with `static` option (which will fail, Haiku
  follows in BeOS footsteps and is all about using shared libraries),
- failing fast for some combinations/features like `DTrace` (N/A on Haiku),
  `pool_classic`, sanitizers.

`systematic-testing` was tested long time ago, but not recently.
Same for using `runtime-bitcode`.
