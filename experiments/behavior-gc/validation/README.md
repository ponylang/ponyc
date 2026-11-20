# Additional configurations

Start from the configured Linux checkout and the temporary directory used in the [native replay](../README.md). The forced extension scripts build on the outputs of that replay. Give them the following directory layout in the temporary directory.

```bash
cp -R experiments/behavior-gc/validation "$trial/extended"
ln -s "$trial/forced" "$trial/extended/verified-source"
ln -s "$trial/forced" "$trial/extended/source-copy"
mkdir -p "$trial/extended/logs"
python3 "$trial/forced/guard.py" timeout 600 bash "$trial/extended/bitcode.sh"
```

Configure ASan and UBSan together with the memalign pool. The two harnesses need the CMake configuration and the resulting compiler, CRT objects and runtime archive. They expect the configuration directory to be named `config`, next to the `debug-*` output directory.

```bash
export ORDINARY_ASAN_BUILD="$trial/extended/asan"
guard="$trial/ordinary/guard.py"
python3 "$guard" timeout 300 cmake --preset debug-asan -B "$ORDINARY_ASAN_BUILD/config"
ASAN_OPTIONS=detect_leaks=0 python3 "$guard" timeout 2400 cmake --build "$ORDINARY_ASAN_BUILD/config" --parallel 2 --target ponyc
python3 "$trial/extended/asan-fixture.py"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python3 "$guard" timeout 600 bash "$trial/extended/asan-fixture/run.sh"
python3 "$guard" timeout 2400 python3 "$trial/ordinary/extra.py"
```

The 32-bit runs cross-generate Pony objects and link them with a runtime compiled on each target. The i386 run needs the Alpine builder image named in `cross.py`. The ARM run creates a temporary directory on an SSH-accessible ARMv7 Linux machine with a native C/C++ toolchain. Set `PONY_ARM_SSH` to its SSH destination. Set `LLVM_OPT` to the LLVM 23.1.3 optimizer if it is installed outside `/usr/sbin/opt`. Run either or both targets.

```bash
python3 "$guard" timeout 1800 python3 "$trial/ordinary/cross.py" i386
python3 "$guard" timeout 1800 python3 "$trial/ordinary/cross.py" arm
```
