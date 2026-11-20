# Threshold-driven GC inside one Pony behavior

A private compiler and runtime collected an actor's heap before its allocating behavior returned, using the ordinary heap threshold. In one regex compilation, collecting after the parser returned let the NFA builder satisfy 54 allocation requests from its recyclable chunk lists. With optimized Pony code, the worker held 309,248 bytes of chunk capacity when construction finished. With collection disabled at the same sites, it held 361,472 bytes. [Phase measurements](phase.csv)

The private compiler inserted polls between synchronous helper calls in a single selected source file. Eligible behaviors took no parameters and consisted entirely of calls to zero-argument, nonpartial methods on the receiver returning `None`. After a helper returned, everything the worker needed across the poll was reachable from actor fields. The runtime checked the current actor, compared its heap charge with its normal collection threshold, and called the existing collector when the tracing state, reference maps and finalizer checks permitted it. [Compiler insertion](../../ordinary/codegen.inc) · [Runtime poll](../../ordinary/actor.inc)

## One regex compilation

The driver called byte-for-byte copies of the package-private `_IRegexpParser.parse` and `_NFABuilder.build`. It retained the AST in an actor field between those calls, then retained the NFA for matching. The two inputs contained 64 and 256 repetitions of `(ab)`. In each case the resulting NFA had the expected state count (129 or 513), accepted the complete input, and rejected both a wrong last character and an extra character. The two collections took place after parsing and after building. The unchanged public `IRegexpCompiler.parse` was exercised separately in batches, with collection between batches. [Phased driver](../../ordinary/phase.py) · [Batched driver](../../ordinary/fixtures.py)

| Pony codegen | Groups | Bytes charged before → after the parse poll | Build-end capacity, GC disabled | Build-end capacity, GC enabled | Recycled chunks selected during build |
| --- | ---: | ---: | ---: | ---: | ---: |
| Debug | 64 | 23,712 → 10,208 | 96,256 B | 82,944 B | 16 |
| Debug | 256 | 94,368 → 40,928 | 377,856 B | 325,632 B | 54 |
| Optimized | 64 | 23,648 → 10,208 | 92,160 B | 78,848 B | 16 |
| Optimized | 256 | 94,304 → 40,928 | 361,472 B | 309,248 B | 54 |

Capacity includes payload space in the worker's active and recyclable chunks. The parse collection lowered the heap charge while leaving capacity unchanged at that point. The builder subsequently allocated from the recyclable lists. The capacity difference in the table was measured on return from the builder, before the next collection or any matching. [Raw large optimized run](artifacts/phase-optimized-observed/large-observed.err) · [Matched control](artifacts/phase-optimized-observed/large-sample.err) · [Capacity and reuse counters](../../ordinary/build.py)

## Batched work and process memory

For each workload, a single behavior completed 8 or 64 batches. The observed and uninstrumented GC-enabled runs recorded one collection per batch inside that behavior. Both the sample-only run and the stock no-poll run recorded zero. Increasing the initial threshold also suppressed all collections inside the large behaviors. Actor ID, thread, paired GC events and the collector's completion record agreed in the observed runs. [Results](measurements.csv) · [Trace checks](../../ordinary/run.py) · [Threshold controls](logs/continued.log)

The table gives the largest worker capacity sampled at a poll and, separately, median process peak RSS from five untraced runs of each optimized large workload. RSS was measured with the clean runtime and the stock baseline.

| Work in one behavior | Worker capacity, disabled → enabled | Process peak RSS, stock → enabled |
| --- | ---: | ---: |
| Compile 512 retained definitions to instructions in each of 64 batches | 3,170,304 → 76,800 B | 6,144 → 3,328 KiB |
| Compile and match 32 regexes in each of 64 batches | 11,694,080 → 185,344 B | 15,104 → 3,584 KiB |
| Compile the PEG package's JSON grammar and parse its test input in each of 64 batches | 41,994,240 → 670,720 B | 46,464 → 4,224 KiB |

The graph driver checked shared binding identity, instruction operands and a checksum. The regex driver checked accepted and rejected strings and used the first compiled regex again in later batches. The grammar driver checked full input consumption and compared the printed tree with the package's expected output using both the fresh parser and the first parser it retained. Deliberately changing the expected results made all eight output controls fail. [Workload sources](../../ordinary/fixtures.py) · [Output controls](../../ordinary/oracle.py) · [Control results](logs/extra.log)

In the five large grammar runs, the GC-enabled times were about 152–153 ms three times and 312–314 ms twice. The stock median was 312.61 ms. With the threshold raised, median peak RSS was 46,336 KiB in both configurations and median elapsed time was about 313 ms in both. These are whole-process measurements on x86-64 WSL. [Native timing and RSS samples](native-timings.csv)

A separate optimized arithmetic behavior measured the cost of inserting polls when no collection occurred. Across 20 alternating pairs of runs, each with 16,384 timed behavior intervals and 65 polls per interval, the median paired increase was 2.15 ns per inserted poll. That includes the effect on generated arithmetic. With poll insertion disabled, the optimizer folded all 64 updates into one multiply. The enabled build executed the updates separately. [Interval measurements](internal-cost.csv) · [Enabled IR](runs.tar.gz) · [Stock IR](runs.tar.gz)

## Controls and coverage

At heap charges above the 16,384-byte threshold, separate runs with a local object-map entry, a foreign-map entry or a pending finalizer each declined collection and preserved the expected output. The otherwise eligible control collected. IR checks covered the selected source file, ineligible behavior bodies, the root-audit option and mutation mode. Deliberately malformed or inconsistent traces and counter records failed verification. [Runtime guard results](logs/gates.log) · [Outputs and thresholds](../../ordinary/refusal.py) · [Compiler and trace controls](../../ordinary/controls.py)

The same actor also recovered after a refusal. Its first behavior encountered a pending finalizer at two over-threshold polls and finished without collecting. The finalizer ran after the resource was released. The next queued behavior collected at its poll and read the expected value, then a third message verified the order. The clean runtime repeated this with one and two schedulers. [Queue and recovery results](logs/recovery.log)

Native x86-64 runs covered both debug and optimized Pony code for all four workloads. For the graph and phased regex drivers, 32 additional enabled runs covered merged runtime bitcode with full and ThinLTO, one or two schedulers, and blocking detection enabled or disabled. Sixteen enabled native ASan/UBSan runs covered both codegen modes and the same scheduler options with leak detection disabled. Each graph trace recorded 64 collections and each phased regex trace recorded two. [Configuration results](logs/extra.log)

Sixteen 32-bit ELF programs ran on i386 Linux in an Alpine container and on a physical armv7l Raspberry Pi. Each target ran the graph and phased drivers in debug and optimized mode, with polls enabled and disabled. In each enabled run the output checks passed and the traces recorded 64 and two collections respectively. Every disabled trace recorded zero. A private host generator used the actor padding measured on each target (188 bytes, with four-byte pointers). The C runtime was compiled on each target. Coverage here is limited to these cross-generated programs. [Target results](logs/cross.log) · [ABI and ELF checks](../../ordinary/cross.py)

The WSL guard required 5.5 GiB of available memory to start a job and stopped it below 5 GiB. The minimum recorded in the primary run log was 13.657 GiB. The i386 container had a 768 MiB memory limit. [Guard](../../ordinary/guard.py) · [WSL samples](logs/memory.tsv) · [Container settings](../../ordinary/cross.py)

The ordinary checkout's earlier regression run passed all 20 core tests and seven tools suites, plus separate optimized full-program and stdlib reruns. Its examples test failed because the multipart-upload example called a partial method without handling the error. Local commit `830ad96dc` corrected the example, and the stock examples CTest then passed with debug Pony codegen. The threshold-driven compiler and runtime changes used for these measurements are available in the branch’s experiment directory. [Earlier regression results](../forced/README.md) · [Examples rerun](logs/examples.log)

A source-only native replay rebuilt the private compiler and runtimes in a fresh directory. All 48 batch records and all eight phase records matched byte for byte. The guard-isolation and queue-recovery results matched as well. The replay measured 2.17 ns per inserted poll in the timed arithmetic fixture. The LTO, sanitizer and 32-bit results above come from separate executions. [Replay log](logs/replay.log) · [Original harness source](original-sources.tar.gz) · [Reproduction instructions](../../README.md) · [Decision log](decisions.md) · [Independent review](review.md)

## Remaining work

The earlier forced-collection experiment preserved a local reference to a mutable Cell containing one U64 at a selected site. The threshold-driven runs here cover actor fields after synchronous helpers returned. Collection with managed locals live in active or deeper call frames still needs a way to publish those roots and tests that use them after collection. Collection across general FFI lifetimes, finalization and cross-actor ownership transfer also remains open. The PEG and regex measurements establish relief for these package workloads. Measuring memory relief for a self-hosted Pony compiler requires running that compiler on a representative compilation. [Earlier forced-collection results](../forced/README.md)
