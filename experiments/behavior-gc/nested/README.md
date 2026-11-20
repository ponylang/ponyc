# Two suspended callers during collection

Two mutable cells survived one collection inside `Probe.phase()`. The behavior `measure()` holds the outer cell and calls `middle()`. The synchronous method `middle()` holds the inner cell and calls `phase()`. Neither caller passes its cell to the callee. Each caller reads and changes its own cell after the call returns.

The behavior audit publishes the outer cell at the call to `middle()`. A compiler overlay extends the same audit to one method, `Probe.middle` in this fixture, so the probe can publish the inner cell at the call to `phase()`. The runner checks both calls in the disassembly. In optimized code the outer call is in `Probe_Dispatch`. The experiment covers these two audited frames and an explicit poll. Finding roots in arbitrary frames remains open. The runner records heap charge and collection counts without measuring elapsed time.

Heap charge was 32 bytes at the outer call and 64 at the inner call. Inside `phase()`, a helper allocated 1,024 distinct temporary cells and returned. At the poll the charge was 32,832 bytes, above the ordinary 16 KiB threshold. With both roots published, the collector traced both cells as mutable. After marking finished and before sweep, the probe checked that each of the two distinct cell addresses was already marked. One collection reduced the charge to 64 bytes.

The probe removed the inner root when `phase()` returned. In `middle()` a fresh cell reused the exact address of a temporary. The inner cell still held 271 at its original address, and `middle()` changed it to 278. When `middle()` returned, the probe removed the outer root. The outer cell still held 137 at its original address, and `measure()` changed it to 142.

Four controls disabled collection, raised the threshold to 1 GiB, withheld the outer root, or withheld the inner root. In each control the charge stayed at 32,832 bytes, the collection count was zero, and the fresh cell had a new address. With either root withheld, the probe refused collection before marking. All five conditions ran with one and two scheduler threads, debug and optimized Pony code, and native and ASan/UBSan builds, for 40 runs. Leak detection was disabled for the sanitizer runs. [Measurements](../results/nested/runs.csv) and [run artifacts](../results/nested/runs.tar.gz).

## Reproduce the runs

Start at the repository root with the [Linux x86-64 debug build and native tools](../README.md#reproduce-the-native-runs) and GNU `objdump`. The runner creates a new output directory for each build.

```bash
scratch=$(mktemp -d)
exp=experiments/behavior-gc
cp "$exp/forced/guard.py" "$scratch/guard.py"
python3 "$scratch/guard.py" timeout 600 python3 -B "$exp/nested/run.py" "$scratch/native"
```

For ASan/UBSan, build the sanitizer compiler and runtime and generate a runtime builder from this checkout's `forced/build.py`. Pass that builder and its matching CMake configuration to the nested runner. [Sanitizer build layout](../validation/README.md).

```bash
cp -R "$exp/validation" "$scratch/validation"
cp -R "$exp/forced" "$scratch/validation/verified-source"
config="$scratch/validation/asan/config"
python3 "$scratch/guard.py" timeout 300 cmake --preset debug-asan -B "$config"
ASAN_OPTIONS=detect_leaks=0 python3 "$scratch/guard.py" timeout 2400 \
  cmake --build "$config" --parallel 2 --target ponyc
python3 "$scratch/validation/asan-fixture.py"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  python3 "$scratch/guard.py" timeout 600 python3 -B "$exp/nested/run.py" \
  "$scratch/sanitized" --runtime-builder "$scratch/validation/asan-fixture/build.py" \
  --config "$config"
```
