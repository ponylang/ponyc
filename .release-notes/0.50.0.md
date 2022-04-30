## Fix compiler crash with exhaustive match in generics

Previously, there was an interesting edge case in our handling of exhaustive match with generics that could result in a compiler crash. It's been fixed.

## Fixed parameter names not being checked

In 2018, when we [removed case functions](https://github.com/ponylang/ponyc/pull/2542) from Pony, we also removed the checking to make sure that parameters names were valid.

We've added checking of parameter names to make sure they match the naming rules for parameters in Pony.

## Fixed bug that caused the compiler to crash

We've fixed a [bug](https://github.com/ponylang/ponyc/issues/4059) that caused a compiler crash.

## Allow overriding the return type of FFI functions

With the approval of [RFC 68](https://github.com/ponylang/rfcs/pull/184), we made FFI declarations mandatory, and with that, also removed the ability to separately specify a return type for an FFI function at the call site, like so:

```pony
use @printf[I32](fmt: Pointer[U8] tag, ...)

actor Main
  new create(env: Env) =>
    @printf[I32]("Hello, world!\n".cstring())
```

Since the return type at the call site needed to match the return type of the declaration, we removed this syntax on the grounds of it being redundant.

That change, however, made it impossible to write safe bindings for type-generic C functions, which might return different types depending on how they are used. Let's imagine an example with a generic list:

```C
struct List;

struct List* list_create();
void list_free(struct List* list);

void list_push(struct List* list, void *data);
void* list_pop(struct List* list);
```

We want to use this API from Pony to store our own `Point2D` struct types. We can write the declarations as follows:

```pony
use @list_create[Pointer[_List]]()
use @list_free[None](list: Pointer[_List])

use @list_push[None](list: Pointer[_List], data: Point)
use @list_pop[NullablePointer[Point]](list: Pointer[_List])

primitive _List

struct Point2D
  var x: U64 = 0
  var y: U64 = 0
```

The problem with these declarations is that the `list_push` and `list_pop` functions can only operate with lists of elements of type `Point2D`. If we wanted to use these functions with different types, we would modify `list_push` to take an element of type `Pointer[None]`, which represents the `void*` type. Until now, however, there was no way of making `list_pop` truly generic:

```pony
use @list_push[None](list: Pointer[_List], data: Pointer[None])
use @list_pop[Pointer[None]](list: Pointer[_List])

// OK
let point_list = @list_create()
@list_push(list, NullablePointer[Point2D].create(Point2D))

// OK
let string_list = @list_create()
@list_push(string_list, "some data".cstring())

// Compiler error: couldn't find 'x' in 'Pointer'
let point_x = @list_pop(point_list)
point.x

// Compiler error: wanted Pointer[U8 val] ref^, got Pointer[None val] ref
let head = String.from_cstring(@list_pop(string_list))
```

With this release, one can make `list_pop` generic by specifying its return type when calling it:

```pony
use @list_pop[Pointer[None]](list: Pointer[_List])

// OK
let point_x = @list_pop[Point](point_list)
point.x

// OK
let head = String.from_cstring(@list_pop[Pointer[U8]](string_list))
```

Note that the declaration for `list_pop` is still needed. The return type of an FFI function call without an explicit return type will default to the return type of its declaration.

When specifying a different return type for an FFI function, make sure that the new type is compatible with the type specified in the declaration. Two types are compatible with each other if they have the same ABI size and they can be safely cast to each other. Currently, the compiler allows the following type casts:

- Any struct type can be cast to any other struct.
- Pointers and integers can be cast to each other.

Specifying an incompatible type will result in a compile-time error

## Don't allow FFI calls in default methods or behaviors

In Pony, when you `use` an external library, you import the public types defined by that library. An exception to this rule is FFI declarations: any FFI declarations in a library will not be visible by external users. Given that the compiler only allows a single FFI declaration per package for any given function, if external FFI declarations were imported, deciding which declaration to use would be ambiguous.

When added to the fact that every FFI function must have a matching declaration, a consequence of not importing external declarations is that if a library contains a public interface or trait with default methods (or behaviors), performing an FFI call on those methods will render them unusable to external consumers.

Consider the following code:

```pony
// src/lib/lib.pony
use @printf[I32](fmt: Pointer[None] tag, ...)

trait Foo
  fun apply() =>
    @printf("Hello from trait Foo\n".cstring())

// src/main.pony
use "./lib"

actor Main is Foo
  new create(env: Env) =>
    this.apply()
```

Up until ponyc 0.49.1, the above failed to compile, because `main.pony` never defined an FFI declaration for `printf`. One way of making the above code compile is by moving the call to `@printf` to a separate type:

```pony
// src/lib/lib.pony
use @printf[I32](fmt: Pointer[None] tag, ...)

trait Foo
  fun apply() =>
    Printf("Hello from trait Foo\n")

primitive Printf
  fun apply(str: String) =>
    @printf(str.cstring())

// src/main.pony
use "./lib"

actor Main is Foo
  new create(env: Env) =>
    this.apply()
```

Given that the original error is not immediately obvious, starting from this release, it is now a compile error to call FFI functions in default methods (or behaviors). However, this means that code that used to be legal will now fail with a compiler error. For example:

```pony
use @printf[I32](fmt: Pointer[None] tag, ...)

actor Main is _PrivateFoo
  new create(env: Env) =>
    this.apply()

trait _PrivateFoo
  fun apply() =>
    // Error: can't call an FFI function in a default method or behavior.
    @printf("Hello from private trait _PrivateFoo\n".cstring())
```

As mentioned above, to fix the code above, you will need to move any FFI calls to external function:

```pony
use @printf[I32](fmt: Pointer[None] tag, ...)

actor Main is _PrivateFoo
  new create(env: Env) =>
    this.apply()

trait _PrivateFoo
  fun apply() =>
    // OK
    Printf("Hello from private trait _PrivateFoo\n")

primitive Printf
  fun apply(str: String) =>
    @printf(str.cstring())
```

We believe that the impact of this change should be small, given the usability problems outlined above.

## Fix concurrency bugs in runtime on Arm

Due to some incorrect memory ordering usage within various atomic operations in the runtime, it was possible for "bad things to happen" on Arm platforms. "Bad things" being violations of invariants in the runtime that could lead to incorrect runtime behavior that could exhibit in a variety of ways.

We've strengthened a number of memory orderings on Arm which should address the issues we have observed.

## Fix a runtime fault for Windows IOCP w/ memtrack messages.

When the runtime is compiled with `-DUSE_MEMTRACK_MESSAGES` on
Windows, the code path for asynchronous I/O events wasn't
initializing the pony context prior to its first use,
which would likely cause a runtime crash or other undesirable
behavior.

The `-DUSE_MEMTRACK_MESSAGES` feature is rarely used, and has
perhaps never been used on Windows, so that explains why
we haven't had a crash reported for this code path.

Now that path has been fixed by rearranging the order of the code.

## Add prebuilt ponyc binaries for Ubuntu 22.04

We've added prebuilt ponyc binaries specifically made to work on Ubuntu 22.04.

You can opt into using the Ubuntu binaries when using ponyup by running:

```bash
ponyup default ubuntu22.04
```

