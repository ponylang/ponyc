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
