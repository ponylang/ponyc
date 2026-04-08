## Fix compiler crash when a behavior satisfies a non-tag interface method

Previously, the compiler would crash with an assertion failure when an actor's behavior was used to satisfy an interface method declared with a `box` or `ref` receiver capability:

```pony
interface IFunBox
  fun box apply(s: String)

actor Main
  let _env: Env
  new create(env: Env) =>
    _env = env
    let x: IFunBox = this
    x("hello")

  be apply(s: String) => _env.out.print(s)
```

This is a valid subtype relationship — a behavior runs with a `tag` receiver, and `box`/`ref` are both subcaps of `tag`, so the contravariant receiver check holds. The crash has been fixed. Code of this shape now compiles and runs correctly.
