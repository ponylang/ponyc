## Split Seq into ReadSeq, Seq, and ResizableSeq

`Seq` required every implementor to provide growth methods even when they made no sense — a fixed-size mutable container can't grow, but it couldn't implement `Seq` without pretending it could. The `Seq` interface has been split into three interfaces:

- `ReadSeq[A]` (unchanged) -- read-only access by index.
- `Seq[A]` (narrowed) -- read and write by index.
- `ResizableSeq[A]` (new) -- read, write, grow, and shrink.

`Array`, `String`, and `List` now implement `ResizableSeq` instead of `Seq`.

Code that uses `Seq` and calls growth methods must change `Seq` to `ResizableSeq`.