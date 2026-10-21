## Add --timeout-factor to pony_test

`--timeout-factor=N` multiplies all `long_test` timeouts by N. Pass it on the command line when running a test binary in a slow environment — a QEMU virtual machine, a loaded CI runner — so that tests get more time without changing any test source code. The default is 1 (no change).

```pony
// A test with a 5-second timeout:
h.long_test(5_000_000_000)
```

```sh
# Run with tripled timeouts:
./my-tests --timeout-factor=3
```
