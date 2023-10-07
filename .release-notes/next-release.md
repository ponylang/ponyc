## Fix broken DTrace support

Quite a while back, we broke the support in our Makefile for building the Pony runtime with support for DTrace. We've fixed that and added tests to assure it builds.

## Fix compilation error when building with pool_memalign in release mode

When attempting to build the Pony runtime with the `pool_memalign` option, users would encounter a compilation error if building a `release` rather than `debug` version of the runtime. We've fixed the compilation error and added CI testing to verify we don't get a regression.

## Fix compiler bug that allows an unsafe data access pattern

In November of last year, core team member Gordon Tisher identified a bug in the type system implementation that allowed sharing of data that shouldn't be shareable.

The following code should not compile:

```pony
class Foo
  let s: String box

  new create(s': String box) =>
    s = s'

  fun get_s(): String val =>
    // this is unsafe and shouldn't be allowed
    recover val s end

actor Main
  new create(env: Env) =>
    let s = String
    s.append("world")
    let foo = Foo(s)
    env.out.print("hello " + foo.get_s())
```

Upon investigation, we found that this bug goes back about 8 or 9 years to the when viewpoint adaptation was introduced into the Pony compiler.

We've fixed the logic flaw and added tests to verify that it can't be reintroduced.

This will potentially break your code if you coded an unsafe recover block that the compiler previously allowed.

