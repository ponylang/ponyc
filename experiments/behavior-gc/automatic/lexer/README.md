# Caller roots in a Pony lexer workload

Two suspended callers each kept a batch of tokens from the [ponycc lexer](https://github.com/jemc/ponycc/tree/a36285efb6eb9b3c3ab62778613781c7b7535420/ponycc/pass/parse) across a third lexing operation. The lexer returns an isolated array with token labels and source positions. Each batch constructor consumes that array into an immutable field. The caller publishes its mutable batch and transfers it to an actor field only after its nested call returns. The later verifier reads both token arrays, their backing storage and the original source text. Polls occur between lexing units inside a single behavior.

All 16 runs produced the same 13-token stream as stock ponyc and a hand-written expectation. The comparison covers the source path, label, byte offset, length and original bytes of each token, including newlines and the offset after a UTF-8 comment. The enabled path collected once. An extra local before the inner declaration made its frame ineligible. A separate variant kept a finalizable object on the actor heap. Both variants completed with zero collections inside the measured behavior. These runs cover actor-local lexing and an isolated-to-immutable handoff. [Measurements](safety.csv)

The fixture generator reads pinned checkouts of ponycc and PEG. It uses ponycc's lexer grammar, lexicon, token flattening and source-position implementation. It substitutes label primitives with the same names for the original generic AST labels to avoid linking later compiler passes. Two range expressions in the source-position display helper use the current standard library spelling. The workload lexes in-memory input.

After the [automatic-poll build](../README.md), set the following paths to clean checkouts and run from the ponyc repository root.

```bash
export PONYCC_DIR=/path/to/ponycc  # a36285efb6eb9b3c3ab62778613781c7b7535420
export PEG_DIR=/path/to/peg        # 810ff6068018a96486a85a2d46e98ceabdeb3ef5
python3 "$scratch/guard.py" timeout 900 python3 -B "$exp/lexer/safety.py" "$scratch/build" "$scratch/two"
```

## Memory while lexing standard-library modules

The lexer processed the complete sources of `builtin/compare.pony`, `builtin/nullable_pointer.pony`, `collections/range.pony` and `collections/ring_buffer.pony`. One pass covered 6,991 source bytes. Four passes ran inside one behavior. Stock and experimental compilers produced byte-identical token streams, including all 4,089 records in the larger run. Each token's text matched its byte range in the source.

| Input in one behavior | Stock end charge | Enabled end charge | Collections inside behavior | Stock median peak RSS | Enabled median peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: |
| 4 module invocations | 1,034,752 B | 19,168 B | 4 | 4,480 KiB | 3,840 KiB |
| 16 module invocations | 4,042,528 B | 19,168 B | 16 | 7,424 KiB | 3,840 KiB |

Both default-threshold runs started at 18,208 charged bytes. With the threshold raised to 1 GiB, stock and experimental builds both started at 71,136 bytes and finished the larger behavior at 4,095,456 bytes. Neither collected during the behavior. GDB read the actor's heap charge at the runtime's behavior entry and return. Peak RSS came from seven separate untraced processes per configuration on x86-64 WSL, with one scheduler and native debug runtime archives for both optimized Pony binaries. The polls ran between module lexes. Parsing, type checking and code generation are outside this measurement. [Managed memory and output hashes](memory.csv) · [Untraced RSS and elapsed time](timing.csv)

```bash
python3 "$scratch/guard.py" timeout 1200 python3 -B "$exp/lexer/memory.py" "$scratch/build" "$scratch/three"
```
