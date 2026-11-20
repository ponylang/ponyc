# A caller's local root during collection in a callee

A mutable `Cell` held in a behavior local survived a collection inside a synchronous helper. The [forced experiment](../forced/) collected at a selected site after the helper returned. The [ordinary-threshold experiment](../ordinary/) collected between helper calls with live objects reachable from actor fields. Here the actor has no fields, and the behavior still needs its local when the helper returns.

`Probe.measure` allocates the cell and calls `phase()` without passing it the cell. At that call site the audit hook publishes the local as a root. Inside `phase()`, another helper allocates 1,024 distinct temporary cells and returns. An explicit poll then checks the actor's heap threshold and collects if collection is enabled and the caller's root is available. The collector traces the published cell as mutable. This fixture covers one caller local containing a leaf object at one known call site. Publishing roots from arbitrary active frames remains open.

The constructor sends `measure` to itself before the actor can block. With a separate send from `Main`, the runtime set the blocked actor's GC threshold to zero before the behavior called the helper. The self-send lets the helper allocations cross the ordinary 16 KiB threshold.

Heap usage rose from 32 to 32,800 bytes. One collection inside `Probe.measure` brought it back to 32 bytes. After `phase()` returned, the caller allocated a replacement in a temporary cell's exact slot. The caller's original cell retained its address and value of 137, which the caller changed to 142. Disabling collection, withholding the published root, or raising the threshold to 1 GiB each left usage at 32,800 bytes with no collection or slot reuse.

All four settings ran with debug and optimized Pony code, with one and two scheduler threads, in both native and ASan/UBSan builds. Leak detection was disabled for the sanitizer runs. The runner checks the native calls in each binary, including the call to the non-inlined `phase()` from the actor dispatch function in optimized code. [Recorded runs](../results/caller/runs.csv)

## Reproduce the native runs

Start at the root of a configured x86-64 Linux checkout with the [debug build and native tools](../README.md#reproduce-the-native-runs), including GNU `objdump`. The runner creates `native` inside the scratch directory and writes the traces, disassembly and per-run measurements there.

```bash
scratch=$(mktemp -d)
cp experiments/behavior-gc/forced/guard.py "$scratch/guard.py"
python3 "$scratch/guard.py" timeout 600 python3 -B experiments/behavior-gc/caller/run.py "$scratch/native"
```

To repeat the sanitizer runs, use the ASan/UBSan compiler and runtime from the [additional configurations](../validation/README.md). Run the commands there through `asan-fixture.py` with this branch's copy of `forced/` as `verified-source`. The generated builder uses the sanitizer configuration's compile flags and archives.

```bash
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python3 "$scratch/guard.py" timeout 600 python3 -B experiments/behavior-gc/caller/run.py "$scratch/sanitized" --builder "$trial/extended/asan-fixture/build.py"
```
