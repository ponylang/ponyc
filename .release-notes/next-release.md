## Fix false "declaration appears after use" error with trait default method bodies

When a type implemented multiple traits that declared the same method, and one of those traits provided a default body, the compiler could reject the program with a false "declaration of 'env' appears after use" error:

```pony
trait Concrete
  fun hello(env: Env) =>
    env.out.print("hello")

trait Abstract1
  fun hello(env: Env)

actor Main is (Abstract1 & Concrete)
  new create(env: Env) =>
    hello(env)
```

Whether the compiler raised the error depended on the order the traits were declared in source. This has been fixed.

