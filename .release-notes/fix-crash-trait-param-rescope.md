## Fix compiler crash with multiple traits sharing a method with a default body

When a type implemented multiple traits declaring the same method and one of those traits provided a default body, the compiler could crash.

```pony
trait Abstract1
  fun hello(env: Env)

trait Abstract2
  fun hello(env: Env)

trait Concrete
  fun hello(env: Env) =>
    env.out.print("hello")

actor Main is (Abstract1 & Abstract2 & Concrete)
  new create(env: Env) =>
    hello(env)
```
