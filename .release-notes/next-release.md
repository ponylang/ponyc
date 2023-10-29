## Disallow `return` at the end of a `with` block

When we reimplemented `with` blocks in 2022, we allowed the usage of `return` at the end of the block. Recent analysis of the generated LLVM flagged incorrect LLVM IR that was generated from such code.

Upon review, we realized that `return` at the end of a `with` block should be disallowed in the same way that `return` at the end of a method is disallowed.

This a breaking change. If you had code such as:

```pony
actor Main
  new create(env: Env) =>
    with d = Disposble do
      d.set_exit(10)
      return
    end
```

You'll need to update it to:

```pony
actor Main
  new create(env: Env) =>
    with d = Disposble do
      d.set_exit(10)
    end
```

The above examples are semantically the same. This also fixes a compiler crash if you had something along the lines of:

```pony
actor Main
  new create(env: Env) =>
    with d = Disposble do
      d.set_exit(10)
      return
    end
    let x = "foo"
```

Where you had code "after the return" which would be unexpected by the compiler.

