# Cost of generated polls with two live callers

Publishing the roots and checking the heap threshold added a median 35 ns per behavior in a short loop with no collections. On the larger ponycc lexer workload, collection between module invocations reduced the measured time by a median 1.10 ms. Both experiments used optimized Pony code, native debug C runtime archives, and one scheduler thread on x86-64 Linux under WSL.

## Short behaviors without collection

The actor ran 65,536 consecutive behaviors. Each kept two cells live across a nested call, updated a checksum, and queued the next behavior until the last round. The timer covered the entire sequence, including message dispatch. The initial GC threshold was 1 GiB. Separate traces confirmed zero collections of the measured actor during the interval, and every run produced the expected checksum.

Each of 24 groups ran three binaries. Stock ponyc generated the baseline. A diagnostic build used a compile-time switch in the same code emitter to generate only the threshold check at the poll site. That check aborts if the actor is over threshold. The full build generated two root pushes, two pops, four calls to claim permission to use the caller's roots, and one poll per behavior. Two claims succeeded (`middle` and `phase`) and two returned without permission (`mix` and `finish`). A separate GDB run found two distinct roots at the poll. Each of the six possible run orders occurred four times. The three median times were 101.23 ns per behavior for stock, 101.93 ns for the threshold check, and 135.33 ns for the full build. [Raw timings](micro.csv)

| Difference within each group | Median (ns/behavior) | 25th to 75th percentile (ns/behavior) |
| --- | ---: | ---: |
| Threshold check minus stock | +2.00 | −4.63 to +7.70 |
| Full build minus threshold check | +32.98 | +28.50 to +40.41 |
| Full build minus stock | +35.04 | +32.22 to +37.72 |

Each row summarizes 24 paired differences. Both tables use inclusive, linearly interpolated quartiles. The full build took longer than the threshold check in all 24 groups. That difference includes root publication and removal, permission checks and the effect of the extra calls on optimization. The timings do not isolate the cost of an individual root push. The middle half of the threshold-check differences spans zero.

## Lexing standard-library source

The actor lexed four standard-library modules four times in one behavior, keeping two small token batches live across all 16 invocations. Timing began just before lexing the outer batch and ended after the nested call returned. The interval includes formatting and writing the module tokens. Printing the two retained batches and any actor-boundary GC after the behavior returned fell outside the interval. All runs produced the same 4,089 token records as the [earlier lexer experiment](../lexer/).

Each of 24 groups ran stock and full builds at both thresholds. Each possible order of the four runs occurred once. Separate traces recorded 16 collections inside the behavior for the full build at the ordinary threshold and zero for the other three configurations. [Raw timings](lexer-time.csv)

| Initial GC threshold | Stock median (ms) | Full build median (ms) | Paired median, full minus stock (ms) | 25th to 75th percentile of paired differences (ms) | Full build faster |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ordinary | 7.143 | 5.944 | −1.103 | −2.113 to −0.448 | 21 of 24 |
| 1 GiB | 7.267 | 6.776 | −0.234 | −0.920 to +0.470 | 16 of 24 |

At the ordinary threshold, the earlier memory runs on this workload found 4,042,528 charged bytes at behavior return for stock and 19,168 for the full build. Median peak RSS was 7,424 KiB and 3,840 KiB respectively. Raising the threshold suppressed collection and both builds finished with 4,095,456 charged bytes. The high-threshold timing differences spanned zero across the middle half of the groups. The workload covers lexing and token output for these inputs. The cost of publishing roots in other caller shapes, parsing, type checking, and code generation remains unmeasured. [Heap charge](../lexer/memory.csv) · [Peak RSS](../lexer/timing.csv)

## Reproduce

Run from the root of this branch on x86-64 Linux with a completed debug build, the Makefiles in `build/build_debug`, native CRT objects, vendored LLVM tools, Clang at `/usr/sbin/clang`, GNU ar and objdump, GDB, GNU timeout, and Python 3 with assertions enabled. The lexer harness reads clean checkouts of ponycc and PEG at the revisions below. The memory guard requires 5.5 GiB available at startup and stops a run below 5 GiB. Use a fresh scratch directory for each run.

```bash
export PONYCC_DIR=/path/to/ponycc  # a36285efb6eb9b3c3ab62778613781c7b7535420
export PEG_DIR=/path/to/peg        # 810ff6068018a96486a85a2d46e98ceabdeb3ef5
scratch=$(mktemp -d)
exp=experiments/behavior-gc/automatic
cp experiments/behavior-gc/forced/guard.py "$scratch/guard.py"
python3 "$scratch/guard.py" timeout 360 python3 -B "$exp/build.py" "$scratch/build"
python3 "$scratch/guard.py" timeout 1200 python3 -B "$exp/cost/run_cost.py" "$scratch/build" "$scratch/four"
python3 "$scratch/guard.py" timeout 1200 python3 -B "$exp/cost/lexer_time.py" "$scratch/build" "$scratch/four-lexer"
```

The scripts write untraced measurements to `$scratch/four/micro.csv` and `$scratch/four-lexer/lexer-time.csv`. The same directories hold the binaries and the separate verification traces.
