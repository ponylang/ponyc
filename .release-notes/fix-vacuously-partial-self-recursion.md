## Fix compiler accepting `?` functions whose only error source is self-recursion

A function marked `?` compiled successfully when its only source of partiality was a recursive call to itself. The partiality was circular — the call raises because the function is partial, and the function is partial because of the call — so no error could ever be raised:

```pony
primitive Foo
  fun apply(x: Bool): Bool ? =>
    apply(not x)?
```

The compiler now rejects this with "function signature is marked as partial but the function body cannot raise an error."

Mutual recursion — A calls B, B calls A, neither with an independent error source — is not yet detected.
