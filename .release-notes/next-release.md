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

## Turn on `verify` pass by default

The `verify` compiler pass will check the LLVM IR generated for the program being compiled for errors. Previously, you had to turn verify on. It is now on by default and can be turned off using the new `--noverify` option.

We decided to turn on the pass for two reasons. The first is that it adds very little overhead to normal compilation times. The second is it will help generate error reports from Pony users for lurking bugs in our code generation.

The errors reported don't generally result in incorrect programs, but could under the right circumstance. We feel that turning on the reports will allow us to find and fix bugs quicker and help move us closer to Pony version 1.

