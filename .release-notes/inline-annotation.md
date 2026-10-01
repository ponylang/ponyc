## Add \inline\, \inline(N)\, and \noinline\ annotations

Three new annotations give control over LLVM's inlining decisions on `fun` methods.

`\inline\` forces the function to be inlined at every call site:

```pony
primitive Foo
  fun \inline\ hot_path(): U64 => 42
```

`\inline(N)\` raises LLVM's inline cost threshold to `N` for the function, making it more likely to be inlined without forcing it:

```pony
primitive Foo
  fun \inline(500)\ fairly_large(): U64 => 42
```

`\noinline\` prevents the function from being inlined:

```pony
primitive Foo
  fun \noinline\ cold_path(): U64 => 42
```

These annotations apply only to `fun` declarations — not to behaviors or constructors. The compiler raises the inline threshold automatically on functions that use direct-call dispatch guards for interfaces; an explicit annotation on such a function overrides that automatic threshold.
