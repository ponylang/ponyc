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

