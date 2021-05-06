## Fix unsound checks for return subtyping

Changed the subtyping model used in the compiler,
fixing some unsound cases that were allowed for function returns.
This also will result in changed error messages, with more
instances of unaliasing instead of aliasing. The tutorial has
been updated with these changes.

This is a breaking change, as some code which used to be accepted
by the typechecker is now rejected. This could include sound code
which was not technically type-safe, as well as unsound code.

This code used to be erroneously allowed and is now rejected
(it would allow a val and an iso to alias):
```
class Foo
  let x: Bar iso = Bar
  fun get_bad(): Bar val =>
    x
```

In addition, the standard library Map class now has a weaker
type signature as it could not implement its current type signature.
The `upsert` and `insert_if_absent` methods now return T! instead of T

## Improve error messages when matching on struct types

A struct type doesn't have a type descriptor, which means that they cannot be used in match or "as" statements. Before this change, the compiler would incorrectly warn that matching against a struct wasn't possible due to a violation of capabilities, which was confusing. With this release, the compiler will now show a more helpful error message, explicitly mentioning that struct types can't be used in union types.

As an example, the following piece of Pony code:

```pony
struct Rect

actor Main
  new create(env: Env) =>
    let a: (Rect | None) = None
    match a
    | let a': Rect => None
    | None => None
    end
```

would fail to compile on ponyc 0.40.0 with the following error message:

```
Error:
main.pony:7:7: this capture violates capabilities, because the match would need to differentiate by capability at runtime instead of matching on type alone
    | let a': Rect => None
      ^
    Info:
    main.pony:5:18: the match type allows for more than one possibility with the same type as pattern type, but different capabilities. match type: (Rect ref | None val)
        let a: (Rect | None) = None
                     ^
    main.pony:7:7: pattern type: Rect ref
        | let a': Rect => None
          ^
    main.pony:7:15: matching (Rect ref | None val) with Rect ref could violate capabilities
        | let a': Rect => None
                  ^
```

Starting with this release, the error message is:

```
Error:
main.pony:7:7: this capture cannot match, since the type Rect ref is a struct and lacks a type descriptor
    | let a': Rect => None
      ^
    Info:
    main.pony:5:18: a struct cannot be part of a union type. match type: (Rect ref | None val)
        let a: (Rect | None) = None
                     ^
    main.pony:7:7: pattern type: Rect ref
        | let a': Rect => None
          ^
    main.pony:7:15: matching (Rect ref | None val) with Rect ref is not possible, since a struct lacks a type descriptor
        | let a': Rect => None
                  ^
```

## Make FFI declarations mandatory (RFC 68)

This release introduces a breaking change for code that uses the C-FFI (Foreign Function Interface). It is now mandatory to declare all FFI functions via `use` statements. In addition, it is now a syntax error to specify the return type of an FFI function at the call site. The tutorial has been updated with these changes.

Where you previously had code like:

```pony
let ptr = @pony_alloc[Pointer[U8]](@pony_ctx[Pointer[None]](), USize(8))
Array[U8].from_cpointer(ptr, USize(8))
```

you now need

```pony
// At the beginning of the file
use @pony_ctx[Pointer[None]]()
use @pony_alloc[Pointer[U8]](ctx: Pointer[None], size: USize)
// ...
let ptr = @pony_alloc(@pony_ctx(), USize(8))
Array[U8].from_cpointer(ptr, USize(8))
```

If you're calling a C function with a variable number of arguments (like `printf`), use `...` at the end of the declaration:

```pony
use @printf[I32](fmt: Pointer[U8] tag, ...)
// ...
@printf("One argument\n".cpointer())
@printf("Two arguments: %u\n".cpointer(), U32(10))
@printf("Three arguments: %u and %s\n".cpointer(), U32(10), "string".cpointer())
```

FFI declarations are visible to an entire package, so you don't need to add type signatures to all Pony files.

## Change the return type of String.add to String iso^ (RFC 69)

This release introduces a breaking change by changing the return type of `String.add` from `String val` to `String iso^`.

Where you previously had code like:

```pony
let c = Circle
let str = "Radius: " + c.get_radius().string() + "\n"
env.out.print(str)
```

you now need:

```pony
let c = Circle
let str = recover val "Radius: " + c.get_radius().string() + "\n" end
env.out.print(str)
```

or you can also let the compiler do the work for you by using explicit type declarations:

```pony
let c = Circle
let str: String = "Radius: " + c.get_radius().string() + "\n"
env.out.print(str)
```

The above code works since `val` is the default reference capability of the `String` type.

The new type makes it simpler to implement the `Stringable` interface by using `String.add`. Where before you had code like:

```pony
class MyClass is Stringable
  let var1: String = "hello"
  let var2: String = " world"

  fun string(): String iso^ =>
    recover
      String.create(var1.size() + var1.size())
        .>append(var1)
        .>append(var2)
    end
```

you can now implement the `string` method as such:

```pony
class MyClass is Stringable
  let var1: String = "hello"
  let var2: String = " world"

  fun string(): String iso^ =>
    var1 + var2
```

## Improve error message when attempting to destructure non-tuple types

Sometimes, the compiler will infer that the return type of an expression is an union or an intersection of multiple types. If the user tries to destructure such result into a tuple, the compiler will emit an error, but it won't show the user what the inferred type is. This could be confusing to users, as they wouldn't know what went wrong with the code, unless they added explicit type annotations to the assigned variables.

Starting with this release, the compiler will now show what the inferred type is, so that the user can spot the problem without needing to explicitly annotate their code.

As an example, the following piece of Pony code:

```pony
actor Main
  new create(env: Env) =>
    (let str, let size) =
      if true then
        let str' = String(5) .> append("hello")
        (str', USize(5))
      else
        ("world", USize(5))
      end
```

would fail to compile on previous releases with the following error message:

```
Error:
main.pony:3:25: can't destructure a union using assignment, use pattern matching instead
    (let str, let size) =
                        ^
```

Starting with this release, the error message will show the inferred type of the expression:

```
Error:
main.pony:3:25: can't destructure a union using assignment, use pattern matching instead
    (let str, let size) =
                        ^
    Info:
    main.pony:4:7: inferred type of expression: ((String ref, USize val^) | (String val, USize val^))
          if true then
          ^
```

## Fix memory corruption of chopped Arrays and Strings

With the introduction of `chop` on `Array` and `String` a few years ago, a constraint for the memory allocator was violated. This resulted in an optimization in the realloc code of the allocator no longer being safe.

Prior to this fix, the following code would print `cats` and `sog`. After the fix, it doesn't corrupt memory and correctly prints `cats` and `dog`.

```pony
actor Main
  new create(env: Env) =>
    let catdog = "catdog".clone().iso_array()
    (let cat, let dog) = (consume catdog).chop(3)
    cat.push('s')
    env.out.print(consume cat)
    env.out.print(consume dog)
```

