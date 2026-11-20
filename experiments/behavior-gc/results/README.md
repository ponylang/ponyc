# Recorded runs

[Threshold-driven collection](ordinary/README.md) reports heap charge, worker chunk capacity, chunk reuse and peak process RSS. [Forced collection](forced/README.md) records what happened to the live local and the dead temporary at a single selected site. The [suspended-caller runs](caller/runs.csv) record caller payloads, slot reuse and collection counts in both codegen modes. The [archive](caller/runs.tar.gz) contains their runtime output, traces and native disassembly. The [two-caller measurements](nested/runs.csv) also record which roots were published at the poll and which were marked before sweep. The low bit identifies the outer caller and the next bit identifies the inner caller. [Two-caller traces, output and disassembly](nested/runs.tar.gz)

Each `runs.tar.gz` contains text evidence. The ordinary archive contains native traces, counters, control outputs, workload inputs, timing-fixture IR, and 32-bit target ABI and ELF header records. The forced archive contains its native, merged-runtime and sanitizer records. The diagnostics archive contains the earlier compiler audit and calibration results. The replay directory holds the independently rebuilt native run. Build artifacts can be regenerated with the harnesses. Absolute checkout and experiment paths in the archived text have been shortened.

The forced and ordinary directories also contain the original harness source used for the recorded runs. The browsable harnesses include fixture documentation and portable replay settings.

Each run archive has a manifest of SHA-256 digests of its members. For example, from the checkout root:

```bash
evidence="$PWD/experiments/behavior-gc/results/ordinary"
unpacked=$(mktemp -d)
tar xzf "$evidence/runs.tar.gz" -C "$unpacked"
(cd "$unpacked" && sha256sum --check "$evidence/runs.tar.sha256")
```

The batch CSV columns are workload, Pony codegen mode, input size, variant, completed collections, maximum sampled chunk capacity, maximum charge before a poll, charge after the final poll, and selections from recyclable chunk lists. The phase CSV columns are codegen mode, input size, variant, charge before and after the parse poll, capacity before and after that poll, capacity at build end before its poll, and recycle selections during the build. Heap sizes are in bytes. The native timing CSV records workload, input size, raised-threshold flag, repetition, variant, elapsed nanoseconds, and maximum child RSS in KiB.
