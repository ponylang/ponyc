## Change Stringable.string() to return String val instead of String iso^

The `Stringable` interface's `string()` method now returns `String val` (written as bare `String`) instead of `String iso^`. The previous return type forced every implementation to allocate a new string even when returning a literal or an existing `val` field, because `iso^` requires an isolated reference that the caller can freely mutate. No caller in the standard library or tools ever used that mutability — every call site consumed the result as `val` immediately.

With `String val`, implementations can return string literals directly with zero allocation. The common case of a type whose `string()` method returns a fixed label drops from two heap allocations to zero.

All code that implements `Stringable` must update its `string()` method signature. The migration is mechanical:

Before:

```pony
primitive Green is Stringable
  fun string(): String iso^ => "green".clone()

class MyClass is Stringable
  let _name: String

  fun string(): String iso^ =>
    recover iso
      String.>append("MyClass(")
        .>append(_name)
        .>append(")")
    end
```

After:

```pony
primitive Green is Stringable
  fun string(): String => "green"

class MyClass is Stringable
  let _name: String

  fun string(): String =>
    recover val
      String.>append("MyClass(")
        .>append(_name)
        .>append(")")
    end
```

Callers of `string()` require no changes — `String val` satisfies every context where `String iso^` was previously used as `val`.

The one case that needs attention: code that consumed the `string()` result as `iso` for in-place mutation. Add `.clone()` at the call site to get a mutable copy. This pattern did not occur in the standard library.
