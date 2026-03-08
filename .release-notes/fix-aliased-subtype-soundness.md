## Fix type system soundness hole

The compiler incorrectly accepted aliased type parameters (`X!`) as subtypes of their unaliased form when used inside arrow types. This allowed code that could duplicate `iso` references, breaking reference capability guarantees. For example, a function could take an aliased (tag) reference and return it as its original capability (potentially iso), giving you two references to something that should be unique.

Code that relied on this — likely by accident — will now get a type error. The most common pattern affected is reading a field into a local variable and returning it from a method with a `this->A` return type:

```pony
// Before: compiled but was unsound
class Container[A]
  var inner: A
  fun get(): this->A =>
    let tmp = inner
    consume tmp

// After: return the field directly instead of going through a local
class Container[A]
  var inner: A
  fun get(): this->A => inner
```

The intermediate `let` binding auto-aliases the type to `this->A!`, and consuming doesn't undo the alias. Returning the field directly avoids the aliasing entirely.

The persistent list in the `collections/persistent` package had four signatures using `val->A!` that relied on this bug. These have been changed to `val->A`. If you had code implementing the same interfaces with explicit `val->A!` types, change them to `val->A`.
