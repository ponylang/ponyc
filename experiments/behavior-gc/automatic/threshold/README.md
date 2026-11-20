# Checking the GC threshold before publishing roots

Checking the heap threshold at the outer call saved a median 31.8 ns per short behavior compared with publishing both callers' roots on every run. But the actor could cross the threshold inside the call. On the ponycc lexer workload, the entry check missed all 16 opportunities to collect. Inlining the two calls let the compiler check after lexing each module and pass both roots to the collector only when due. This version collected 16 times and finished with 19,168 charged bytes, down from 4,042,528 with the entry check.

## Where the check runs

The experiment recognizes the [two-caller shape](../) in one selected source file. The behavior holds one local across a call to `middle`, which holds another across a call to `phase`. Both callers use their locals after the calls return. Five builds ran the same inputs.

| Variant | Code at the measured site |
| --- | --- |
| Stock | Ordinary ponyc with actor-boundary collection. |
| Eager | Each caller publishes its root before the nested call and removes it on return. The callee claims permission to use the published roots. The poll inside `phase` checks whether collection is due. |
| Entry | Before calling `middle`, compare the actor's charged bytes with its threshold. If already over, publish the outer root and call a copy of `middle` with eager root handling. Otherwise call the uninstrumented method. Both paths retain the native calls to `middle` and `phase` in the checked fixture. |
| Late | Inline `middle` and `phase` into the eligible caller. At the poll inside the inlined `phase`, compare the charge with the threshold. Only when over threshold, load the two roots and pass them directly to the runtime. |
| Inline control | Perform the same two inlinings as Late and omit the poll. |

Late can inline the outer call to `middle` even if the inner call is ineligible. It emits a poll only after inlining both calls.

## Short behaviors without collection

One actor ran 65,536 behaviors with a 1 GiB initial threshold. The timer included dispatch. Every run produced the expected checksum. A separate trace for each build showed zero collections of the measured actor during the interval.

The tables below give the first run, before the compiler excluded local declarations inside constructor arguments. A full replay with that restriction used the same workloads and run orders.

The timing runs used optimized Pony code, native debug runtime archives and one scheduler thread on x86-64 Linux under WSL. After an untimed warmup for each configuration, each of 30 groups ran all five builds. The orders balanced each build's position and its immediate predecessor. The lexer used the same design at each threshold, alternating which threshold ran first. Each difference below compares runs from the same group. Brackets give the 25th and 75th percentiles of the paired differences using inclusive linear interpolation. [Short-behavior timings](micro.csv)

| Paired difference | Median [25th, 75th percentile] (ns/behavior) |
| --- | ---: |
| Eager minus stock | +36.26 [+30.20, +43.89] |
| Entry minus stock | +2.42 [−3.16, +10.35] |
| Late minus stock | +0.23 [−2.44, +6.34] |
| Entry minus eager | −31.79 [−35.93, −24.81] |
| Late minus eager | −35.66 [−41.10, −31.78] |
| Late minus inline control | +1.78 [−2.95, +9.99] |

Both threshold-first variants ran faster than eager in all 30 groups. Comparing Late with the inline control holds the two inlinings constant. The middle half of those differences spans zero.

In the replay, eager took longer than stock in all 30 groups, by a median 35.85 ns per behavior. The paired differences against stock were +1.39 ns [−3.22, +6.45] for Entry and −1.74 ns [−7.88, +3.99] for Late. Late minus the inline control was −0.81 ns [−7.82, +4.07]. [Replay timings](replay/micro.csv)

## Lexing standard-library source

The actor retained two small token batches while lexing four modules four times each in one behavior. Timing started just before lexing the outer batch and stopped when the nested call returned. The interval included formatting and writing module tokens. Printing the retained batches and actor-boundary GC occurred outside it. Each timed run produced the same 4,089 token records, with SHA-256 `a1581d0dbb9515f1aaf1c61b548cb96090fa9eb2ada4e37a4e03577705d16f59`.

At the ordinary threshold, separate runs counted collections inside the behavior and measured the actor's heap charge at return. Peak RSS is the median for the whole process across the 30 timing runs. [Heap charge and collections](lexer-memory.csv) · [Timings and peak RSS](lexer.csv)

| Variant | Collections | Charge at return (bytes) | Peak RSS (KiB) | Median time (ms) |
| --- | ---: | ---: | ---: | ---: |
| Stock | 0 | 4,042,528 | 7,424 | 6.448 |
| Eager | 16 | 19,168 | 3,840 | 5.687 |
| Entry | 0 | 4,042,528 | 7,424 | 6.603 |
| Late | 16 | 19,168 | 3,840 | 5.540 |
| Inline control | 0 | 4,042,528 | 7,552 | 6.496 |

| Paired difference | Ordinary threshold, median [25th, 75th percentile] (ms) | 1 GiB threshold, median [25th, 75th percentile] (ms) |
| --- | ---: | ---: |
| Entry minus stock | +0.082 [−0.108, +0.338] | +0.044 [−0.326, +0.322] |
| Late minus stock | −0.847 [−1.125, −0.500] | −0.024 [−0.312, +0.323] |
| Late minus eager | −0.077 [−0.449, +0.201] | +0.067 [−0.145, +0.532] |
| Late minus inline control | −0.877 [−1.160, −0.478] | +0.020 [−0.296, +0.434] |

At 1 GiB, every build ran without an in-behavior collection and finished with 4,095,456 charged bytes. Median peak RSS ranged from 7,424 to 7,552 KiB. These measurements cover the selected two-root call chain and the lexer timing interval. The cost of inlining other methods and the effect on a complete compiler run remain unmeasured.

The replay produced the same 4,089 token records, heap charges and collection counts. At the ordinary threshold, Late minus stock was −0.839 ms [−1.193, −0.424]. Median peak RSS was again 7,424 KiB for stock and 3,840 KiB for Late. [Replay timings and peak RSS](replay/lexer.csv)

## Threshold crossings and refusals

In a separate fixture, the entry check ran at 160 charged bytes against a 16,384-byte threshold. The actor entered `fill` at 288 bytes and returned from it at 65,888. Entry had already selected the uninstrumented call. Late checked at 65,888 bytes, collected, and entered `finish` at 288. When the outer local alone put the actor over threshold before the call (65,632 bytes), both variants collected. With the threshold raised to 1 GiB, neither collected. Setting the threshold to exactly the charge at the check in GDB also left both checks untaken. [Charge at each check and call](crossings.csv)

The replay recorded the same charge and threshold at every observed call.

An unsupported behavior with an additional live local called the same methods and crossed the threshold from 416 to 66,016 bytes. Neither variant checked or collected on that path. GDB showed native `middle` and `phase` frames there in both builds. On the supported path, those frames were present for Entry and absent for Late. Separate runs with debug and optimized Pony code verified both callers' results and observed marks for both holders, their arrays and both backing allocations before sweep. [Collection counts](safety.csv)

A constructor argument can declare a local that remains in scope after `middle` returns. The compiler excludes any initializer containing a local declaration. A fixture declared an integer in the outer constructor argument and read it after the call. Both optimized builds omitted the threshold check at that site and ran without an in-behavior collection. [Fixture and native checks](checks.py)

Two more optimized lexer fixtures exercised Late. With an unsupported inner caller, the runtime collection helper was never called. With a pending finalizer, the helper ran at 40,544 charged bytes against a 36,480-byte threshold and returned without collecting. Both produced the expected tokens. The helper also checks the current actor, tracing state, reference maps and ownership of both roots before collection. [Runtime checks](runtime.inc)

## Reproduce

Run from the repository root on x86-64 Linux with the native debug build and tools listed in the [earlier cost experiment](../cost/#reproduce). The harness needs clean checkouts of ponycc and PEG at the revisions below. The guard requires 5.5 GiB available at startup and stops a run below 5 GiB.

```bash
export PONYCC_DIR=/path/to/ponycc  # a36285efb6eb9b3c3ab62778613781c7b7535420
export PEG_DIR=/path/to/peg        # 810ff6068018a96486a85a2d46e98ceabdeb3ef5
scratch=$(mktemp -d)
cp experiments/behavior-gc/forced/guard.py "$scratch/guard.py"
python3 "$scratch/guard.py" timeout 1200 python3 -B experiments/behavior-gc/automatic/threshold/benchmark.py "$scratch"
```

The harness writes the two timing CSVs under `$scratch/micro` and `$scratch/lexer`, the lexer heap measurements under `$scratch/lexer`, and the crossing and refusal checks under `$scratch/checks`.
