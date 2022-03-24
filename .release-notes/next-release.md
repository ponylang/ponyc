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

