## Split Seq into ReadSeq, Seq, and ResizableSeq

`Seq` required every implementor to provide growth methods even when they made no sense — a fixed-size mutable container can't grow, but it couldn't implement `Seq` without pretending it could. The `Seq` interface has been split into three interfaces:

- `ReadSeq[A]` (unchanged) -- read-only access by index.
- `Seq[A]` (narrowed) -- read and write by index.
- `ResizableSeq[A]` (new) -- read, write, grow, and shrink.

`Array`, `String`, and `List` now implement `ResizableSeq` instead of `Seq`.

Code that uses `Seq` and calls growth methods must change `Seq` to `ResizableSeq`.

## Fix release build link failure when building with GCC

Building ponyc with GCC in release mode failed at two points: the ponyc binary itself failed to link, and programs compiled by a GCC-built ponyc failed to link with undefined runtime symbols. Both had the same cause — GCC's LTO format (GIMPLE) is not compatible with LLVM tools. The build system was enabling LTO globally regardless of compiler, producing GIMPLE objects that the embedded `ld.lld` linker and `llvm-ar` archiver could not read. LTO is now disabled entirely for GCC builds.

## Remove compile-time error for constant integer division by zero

The compiler no longer rejects constant integer division or remainder by zero. `let x: I64 = 1 / 0` was a compile error, but `let x = I64(1) / I64(0)` was not — the check only caught bare literals.

## Fix I128 constant division by -1 producing wrong result

`let r: I128 = 0 / -1` produced `I128.min_value()` instead of `0`. The compiler's constant-folding path compared 128-bit values through 64-bit intermediaries, and `I128.min_value()` has all-zero lower 64 bits — making `I128(0)` and `I128.min_value()` look equal. Any I128 constant that is a multiple of 2^64 divided by -1 was affected.

