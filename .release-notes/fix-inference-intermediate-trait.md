## Fix type argument inference through intermediate traits

When a class implemented a generic trait through an intermediate trait rather than directly, the compiler could not infer the type argument at call sites.

```pony
trait Base[T]
  fun value(): T

trait Middle is Base[String]

class Impl is Middle
  fun value(): String => "hello"

primitive Runner
  fun run[T](b: Base[T]): T => b.value()

// Before: required explicit type argument
Runner.run[String](Impl)

// After: T inferred as String through Middle
Runner.run(Impl)
```

This also means `Property2`, `Property3`, `Property4`, `IntProperty`, and `IntPairProperty` subclasses no longer need explicit type arguments when registering with `test.property()`.
