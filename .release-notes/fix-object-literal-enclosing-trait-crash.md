## Fix compiler crash when object literal implements enclosing trait

The compiler crashed with an assertion failure when an object literal inside a trait method implemented the same trait. This code would crash the compiler regardless of whether the trait was used:

```pony
trait Printer
  fun double(): Printer =>
    object ref is Printer
      fun apply() => None
    end
  fun apply() => None
```

This is now accepted. The object literal correctly creates an anonymous class that implements the enclosing trait, and inherited methods that reference the object literal work as expected.
