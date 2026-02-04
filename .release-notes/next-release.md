## Add Alpine 3.23 support

We've added support for Alpine 3.23. Builds will be available for both arm64 and amd64.

## Fix crash when ephemeral type used in parameter with default argument

We've fixed an error where attempting to assign an ephemeral type to a variable caused an assertion failure in the compiler.

The following code will now cause the pony compiler to emit a helpful error message:

```pony
class Foo

actor Main
  fun apply(x: Foo iso^ = Foo) => None

  new create(env: Env) => None
```

```quote
Error:
main.pony:4:16: invalid parameter type for a parameter with a default argument: Foo iso^
  fun apply(x: Foo iso^ = Foo) => None
               ^
```

## Update Docker Image Base to Alpine 3.23

We've updated the base image for our ponyc images from Alpine 3.21 to Alpine 3.23.

## Fix incorrect array element type inference for union types

Due to a small bug in the type system implementation, the following correct code would fail to compile.  We have fixed this bug so it will now compiler.

```pony
type Foo is (Bar box | Baz box | Bool)

class Bar
  embed _items: Array[Foo] = _items.create()

  new create(items: ReadSeq[Foo]) =>
    for item in items.values() do
      _items.push(item)
    end

class Baz

actor Main
  new create(env: Env) =>
    let bar = Bar([
      true
      Bar([ false ])
    ])
    env.out.print("done")
```

## Fix segfault when lambda captures uninitialized field

Previously, the following illegal code would successfully compile, but produce a segfault on execution.

```pony
class Foo
  let n: USize

  new create(n': USize) =>
    n = n'

class Bar
  let _foo: Foo
  let lazy: {(): USize} = {() => _foo.n }

  new create() =>
    _foo = Foo(123)

actor Main
  let _bar: Bar = Bar

  new create(env: Env) =>
    env.out.print(_bar.lazy().string())
```

Now it correctly refuses to compile with an appropriate error message.

```quote
Error:
main.pony:9:34: can't use an undefined variable in an expression
  let lazy: {(): USize} = {() => _foo.n }
                                 ^
```

## Fix compiler crash when assigning ephemeral capability types

Assigning ephemeral capability to a variable was causing the compiler to crash. We've updated to provide a descriptive error message.

Where previously code like:

```pony
actor Main
  new create(env: Env) => 
    let c: String iso^ = String
```

caused a segfault, you will now get a helpful error message instead:

```quote
Error:
/tmp/main.pony:3:24: Invalid type for field of assignment: String iso^
    let c: String iso^ = String
```

