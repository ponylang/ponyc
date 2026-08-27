## Fix compiler crash from return inside resolved iftype branch

Using `return` inside an `iftype` branch caused the compiler to crash:

```pony
primitive Foo[A: Seq[B] ref, B: Comparable[B] #read]
  fun apply(a: A) =>
    iftype A <: Array[B] then
      return None
    end
    None
```

```
LLVM ERROR: Broken module found, compilation aborted!
```

The same crash occurred when the `iftype` was inside a `recover` block.

This has been fixed.
