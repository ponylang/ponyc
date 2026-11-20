# Garbage collection inside a behavior

Can a Pony actor reclaim memory from one phase of a behavior and reuse it in the next? In one regex compilation, the parser left its syntax tree in an actor field and returned. The worker was over its ordinary heap threshold, so the experimental runtime collected before the NFA builder ran. With 256 groups and optimized Pony code, the builder selected recyclable chunks 54 times. At the end of the build, the worker held 309,248 bytes of chunk capacity with collection enabled and 361,472 bytes with collection disabled at the same sites. The NFA passed the same matching checks in both runs.

A second worker compiled a PEG grammar and parsed its test input in each of 64 batches inside one behavior. On x86-64 WSL the median peak RSS across five optimized runs fell from 46,464 KiB in the baseline with poll insertion disabled to 4,224 KiB with collection between batches. Raising the threshold suppressed collection, and both medians were then 46,336 KiB. [Recorded results](results/)

The branch's `--gc-root-audit` flag reports frame roots. Collection requires a separate experimental build. The [threshold-driven experiment](ordinary/) builds a compiler that inserts polls between synchronous helper calls in one selected source file. Eligible behaviors take no arguments and consist entirely of calls on the receiver to zero-argument, nonpartial methods returning `None`. In these fixtures, the live managed objects at each poll are reachable from actor fields. The experimental runtime checks the heap threshold, tracing state, reference maps and pending finalizers before collecting.

The [forced-collection experiment](forced/) uses the audit compiler with a separate runtime that collects at one selected site. A local reference kept a mutable `Cell` alive. Its identity and `U64` value survived across the collection. The next allocation reused the exact 32-byte slot of a temporary from a helper that had returned. The [suspended-caller experiment](caller/) uses the same compiler to publish a behavior's local before a synchronous call. Inside that call, 1,024 temporary cells took the actor over its ordinary GC threshold. Collection at an explicit poll reduced heap charge from 32,800 bytes to 32. The caller then read and updated its local. Automatic poll placement for arbitrary caller frames and a self-hosted Pony compiler workload remain open.

## Reproduce the native runs

Run these commands from the root of a Linux x86-64 checkout of this branch with the debug CMake build complete. The harness uses the Makefiles under `build/build_debug`, vendored LLVM tools, native CRT objects, Clang at `/usr/sbin/clang`, `cc`, GNU `ar`, and Python 3 with assertions enabled. The threshold-driven experiment also reads a clean [PEG checkout](https://github.com/ponylang/peg) at `$PEG_DIR` (default `~/code/ponylang/peg`), pinned to `810ff6068018a96486a85a2d46e98ceabdeb3ef5`. Each run writes its binaries, traces and measurements into its own temporary directory. The memory guard requires 5.5 GiB available at startup and stops the run below 5 GiB.

```bash
trial=$(mktemp -d)
cp -R experiments/behavior-gc/ordinary "$trial/ordinary"
python3 "$trial/ordinary/guard.py" timeout 2400 bash "$trial/ordinary/reproduce.sh"
python3 "$trial/ordinary/guard.py" timeout 120 python3 "$trial/ordinary/gates.py"
python3 "$trial/ordinary/guard.py" timeout 120 python3 "$trial/ordinary/recovery.py"

cp -R experiments/behavior-gc/forced "$trial/forced"
python3 "$trial/forced/guard.py" timeout 2400 bash "$trial/forced/run.sh"
```

The native threshold-driven replay reproduced all 48 batch records and all eight phase records byte for byte. Separate runs covered merged runtime bitcode, ASan/UBSan with leak detection disabled, and 32-bit programs on i386 Linux and an ARMv7 Raspberry Pi. The checkout's 20 core tests and seven tool suites passed, as did optimized stdlib and full-program reruns. The examples test passed after a fix to the multipart-upload example's call to a partial method.

See [additional configurations](validation/README.md) for the merged runtime, sanitizers and 32-bit fixtures.
