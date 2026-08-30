## Fix object literal compilation with union-constrained type parameters

Object literals inside methods whose type parameters have union constraints with mixed capabilities failed to compile:

```pony
interface box FnBox[A, B]
  fun apply(a: A): B ?
interface ref FnRef[A, B]
  fun ref apply(a: A): B ?
type Fn[A, B] is (FnBox[A, B] box | FnRef[A, B] ref)

actor Main
  new create(env: Env) => None
  fun bar[A, B, F: Fn[A, B]](f: F) =>
    object ref
      fun ref foo(a: A) ? =>
        iftype F <: FnBox[A, B] box then f(consume a)?
        elseif F <: FnRef[A, B] ref then f(consume a)?
        else error
        end
    end
```

The compiler reported "type argument is outside its constraint" for the object literal's captured type parameters. The same code without the object literal compiled correctly.

This has been fixed. Object literals inside methods with union-constrained type parameters now compile correctly.
