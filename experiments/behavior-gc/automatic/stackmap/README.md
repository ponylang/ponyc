# Recovering suspended callers' roots from LLVM stack maps

The ponycc lexer collected after each of 16 module invocations while keeping two token batches live in separate native caller frames. LLVM recorded the stack slot for each batch. The runtime walked the frames only when the actor crossed its GC threshold. At behavior return the actor held 19,168 charged bytes, down from 4,042,528 with stock ponyc, and produced the same 4,089 token records. On short behaviors without collection, the median time per behavior differed from stock by −1.73 ns in one build and +1.57 ns in a second build.

## From a return address to a root

The compiler recognizes the [same two-caller shape](../) used for eager root publication. The behavior holds one object across a direct call to `middle`, which holds another across a direct call to `phase`. Both callers use their objects after the calls return. The compiler emits an LLVM stack map with the address of each local's slot before and after its call. The pre-call record keeps the slot materialized before the caller suspends. The post-call record identifies the slot at the return address. The harness checks each return address against the disassembly and checks the store into the slot before the call.

Inside `phase`, a poll compares the actor's charged bytes with its threshold. When the charge is greater, the runtime unwinds the stack. It matches the raw return PC of each of three consecutive frames against an LLVM stack-map record, in the order below.

| Frame | Record at the return PC | What the runtime recovers |
| --- | --- | --- |
| `phase` | 4, after the call to the collection helper | The poll site |
| `middle` | 2, after the call to `phase` | The inner caller's root slot |
| Behavior | 3, after the call to `middle` | The outer caller's root slot |

A missing record or an unexpected role ends the walk before either slot is read. For each caller, the runtime checks the slot's bounds and alignment. It reads the two root pointers only after matching all three frames. The collection helper checks the current actor, tracing state, reference maps, pending finalizers and ownership of both roots. The prototype scans the stack-map table once per frame.

In the optimized short-behavior fixture, LLVM placed the two roots at offsets from the frame pointer (RBP). In the optimized lexer, both callers realigned the stack to 32 bytes and each root was at RSP + 24 in its own frame. At every one of the 16 lexer polls, GDB compared the unwound frame and stack pointers with the caller's registers recorded at callee entry. Both slot addresses matched. Each recovered pointer matched an allocation returned in its own caller and had the `Batch` descriptor. [Lexer map records](lexer-sites.csv) · [Checks for all 32 slots](lexer-slots.csv)

The position-independent executable needs relocations for the function addresses in the maps. The harness recompiles one LLVM MC source file, `MCObjectFileInfo.cpp`, with the ELF stack-map section marked writable and replaces that object in a scratch copy of `libLLVMMC.a`. A linker script puts the maps in `.data.rel.ro` so the loader can relocate them before making them read-only. The harness checks that the table lies inside GNU RELRO and that the executable has no text relocations. [Compiler build](../build.py) · [Linker script](maps.ld)

## Short behaviors without collection

Each timing run used one scheduler thread, optimized Pony code and the native debug runtime on x86-64 Linux under WSL. One actor ran 65,536 short behaviors at a 1 GiB initial threshold. The timer included dispatch. Each run checked the final checksum, and separate traces checked for collections during the measured interval.

| Variant in the CSVs | Code at the measured site |
| --- | --- |
| `stock` | Stock ponyc with actor-boundary collection. |
| `eager` | Each caller publishes its root before the call and removes it on return. The poll can use both published roots. |
| `mapped` | Each caller has a mapped root slot. The poll calls the unwinder only above threshold. |
| `maps` | Both callers have mapped slots. There is no poll in `phase`. |
| `check` | The poll calls the threshold predicate. There are no maps and no call to the unwinder. |

The last three variants retain the two native calls. The compiler disables inlining and tail calls for the participating functions, keeps frame pointers and requests unwind tables. Comparing either control with stock therefore includes the cost of imposing that frame shape.

Each of two series compiled fresh executables with the same compilers. After warmup, each series ran 30 blocks, each containing all five variants in orders balanced for position and immediate predecessor. The table gives the median difference within blocks in ns per behavior. Brackets give the 25th and 75th percentiles of those differences, using inclusive linear interpolation. [First series](micro-1.csv) · [Second series](micro-2.csv)

| Paired difference | First series | Second series |
| --- | ---: | ---: |
| Mapped minus stock | −1.73 [−7.91, +3.19] | +1.57 [−0.66, +3.81] |
| Maps only minus stock | +0.15 [−8.72, +3.44] | +0.75 [−2.08, +3.88] |
| Check only minus stock | +0.50 [−4.57, +5.88] | +3.15 [−1.21, +9.00] |
| Mapped minus maps only | −0.05 [−4.95, +2.55] | +1.03 [−1.55, +5.16] |
| Mapped minus check only | −1.19 [−3.65, +1.78] | −1.59 [−6.76, +3.07] |
| Mapped minus eager | −35.13 [−39.43, −32.22] | −33.94 [−39.36, −28.74] |

Mapped ran faster than eager in all 60 blocks. The middle half of the paired differences between mapped and stock spanned zero in each series.

## Lexing standard-library source

The actor retained two small batches while lexing four modules four times each inside one behavior. The timer included lexing the outer batch and formatting and writing the module tokens. It stopped when the nested call returned, before printing the retained batches and actor-boundary collection. Each of 30 blocks ran all five variants at each threshold, alternating which threshold ran first. Every timed run produced the same token output. Separate runs measured heap charge and counted collections. Peak RSS covers the whole process. [Timings and peak RSS](lexer.csv) · [Token counts, charge and collections](lexer-memory.csv)

| Variant | Collections at ordinary threshold | Charge at return (bytes) | Median peak RSS (KiB) | Median lexer time (ms) |
| --- | ---: | ---: | ---: | ---: |
| Stock | 0 | 4,042,528 | 7,424 | 6.503 |
| Eager | 16 | 19,168 | 3,840 | 5.574 |
| Maps only | 0 | 4,042,528 | 7,680 | 6.639 |
| Check only | 0 | 4,042,528 | 7,680 | 6.537 |
| Mapped | 16 | 19,168 | 4,096 | 5.706 |

At the ordinary threshold the mapped lexer finished a median 0.851 ms sooner than stock. At 1 GiB every variant finished with 4,095,456 charged bytes and zero in-behavior collections. These timings cover the measured lexer interval on this platform and caller shape. The experiment does not measure a complete compiler run or the cost of scanning larger stack-map tables.

| Paired difference | Ordinary threshold, ms | 1 GiB threshold, ms |
| --- | ---: | ---: |
| Mapped minus stock | −0.851 [−1.005, −0.528] | +0.034 [−0.283, +0.436] |
| Mapped minus eager | +0.053 [−0.108, +0.336] | +0.098 [−0.263, +0.349] |
| Mapped minus maps only | −0.855 [−1.146, −0.683] | +0.109 [−0.103, +0.274] |
| Mapped minus check only | −0.872 [−1.229, −0.615] | −0.007 [−0.245, +0.260] |

## Threshold crossings and refusals

In a separate fixture, the actor entered `fill` at 288 charged bytes and reached the poll at 65,888 against a threshold of 16,384. With both debug and optimized Pony code, GDB matched the recovered roots to the callers' allocation results and observed marks for both holders, both arrays and both backing allocations before sweep. Both callers used their objects after collection. At a threshold of 1 GiB, and with the threshold set equal to the charge at the poll, the runtime skipped the walk. [Walks and recovered roots](walks.csv) · [Collection counts](safety.csv)

An unsupported outer behavior with an additional live local reached the same poll at 66,016 bytes. The runtime matched the poll and inner caller, encountered an unknown outer return PC and skipped collection. In another run, changing the inner PC passed to lookup by one byte caused a miss and the walk returned without recovering roots. Both cases ran with debug and optimized Pony code.

Two optimized lexer fixtures also reached the slow path. Adding a local before the inner caller's batch removed its map and the walk stopped at that frame. A pending finalizer allowed all three frames to match, but the collection helper returned at 40,544 bytes against a 36,480-byte threshold. Both fixtures produced the expected tokens and recorded zero in-behavior collections.

## Reproduce

Run from the repository root on x86-64 Linux with the [threshold experiment's tools and native debug build](../threshold/#reproduce), GNU nm and readelf, and a completed vendored LLVM build. The harness reads the LLVM MC compile flags from `build/build_libs/llvm/src/llvm/lib/MC/CMakeFiles/LLVMMC.dir/flags.make` and copies `build/libs/lib/libLLVMMC.a`. It also needs clean checkouts of ponycc and PEG at the revisions below. The memory guard requires 5.5 GiB available at startup and stops a run below 5 GiB.

```bash
export PONYCC_DIR=/path/to/ponycc  # a36285efb6eb9b3c3ab62778613781c7b7535420
export PEG_DIR=/path/to/peg        # 810ff6068018a96486a85a2d46e98ceabdeb3ef5
scratch=$(mktemp -d)
cp experiments/behavior-gc/forced/guard.py "$scratch/guard.py"
python3 "$scratch/guard.py" timeout 2400 python3 -B experiments/behavior-gc/automatic/stackmap/experiment.py "$scratch"
```

The harness writes `micro.csv` in each of `$scratch/micro-1` and `$scratch/micro-2`. The lexer measurements go in `$scratch/lexer`, the lexer maps and slot checks in `$scratch/lexer/mapped`, and the safety and walk results in `$scratch/safety`.

In a replay with two more 30-block series, mapped minus stock was +0.05 [−3.69, +3.65] and +1.00 [−5.29, +3.21] ns per behavior. Mapped ran faster than eager in 59 of the 60 replay blocks. All five lexer variants reproduced their token counts, collection counts and heap charges at both thresholds. [First replay series](replay/micro-1.csv) · [Second replay series](replay/micro-2.csv) · [Lexer timings](replay/lexer.csv) · [Lexer memory](replay/lexer-memory.csv)
