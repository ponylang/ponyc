## Fix compiler crash when combining traits with abstract and default method

When a type implemented two traits that declared the same method — one without a body and one with a default body — the compiler crashed instead of compiling the program. The crash depended on the order of traits in the provides list: it occurred when the trait without a body appeared before the trait with a default body.

```pony
// no_body.pony
trait HasHello
  fun hello(env: Env)

// greeting.pony
trait Greeting
  fun hello(env: Env) =>
    env.out.print("hello!")

// main.pony — crashed with `(HasHello & Greeting)`, compiled fine with `(Greeting & HasHello)`
actor Main is (HasHello & Greeting)
  new create(env: Env) =>
    hello(env)
```
