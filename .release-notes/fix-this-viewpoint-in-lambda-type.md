## Fix incorrect code generation for `this->` in lambda type parameters

When a lambda type used `this->` for viewpoint adaptation (e.g., `{(this->A)}`), the compiler desugared it into an anonymous interface where `this` incorrectly referred to the interface's own receiver rather than the enclosing class's receiver. This caused wrong vtable dispatch, incorrect results, or segfaults when the lambda was forwarded to another function.

```pony
class Container[A: Any #read]
  fun box apply(f: {(this->A)}) =>
    f(_value)
```

The desugaring now correctly preserves the polymorphic behavior of `this->` across different receiver capabilities.
