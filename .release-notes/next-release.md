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

