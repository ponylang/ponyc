## Fix type parameter bugs in generic methods

When a class, actor, primitive, or object literal inherited a generic default method from a trait or interface, and the method had a type parameter with the same name as one of the inheriting type's type parameters, the compiler could treat the two as the same type parameter. Programs that should compile were rejected with `type does not implement its provides list`, explained by lines such as `Into #any ^ is not a subtype of Into #any ^` or `Any val^ is not a subtype of R val ^`. The workaround was to rename one of the type parameters.

```pony
trait Events[T: Any val]
  fun ref get_event(): T

  fun ref map[R: Any val](f: {(T): R} val): Events[R] =>
    Mapped[T, R](this, f)

// Mapped's R and map's R are different type parameters.
class Mapped[T: Any val, R: Any val] is Events[R]
  let _from: Events[T]
  let _f: {(T): R} val

  new create(from: Events[T], f: {(T): R} val) =>
    _from = from
    _f = f

  fun ref get_event(): R =>
    _f(_from.get_event())
```

This program used to fail to compile. It now compiles.

The same kind of name clash caused other failures, which are also fixed:

- When the inherited method used its type parameter inside a lambda or object literal, the program was rejected with `can't reuse name`.
- A default method whose body or default arguments contained a generic lambda, or an object literal with a generic method, behaviour or constructor, was rejected with `can't reuse name` when that lambda's or method's type parameter had the same name as one of the inheriting type's.
- A generic default method from a trait in another package was rejected when its type parameter had the same name as a type in the inheriting type's package, with `type parameter shadows existing type` or `can't reuse name`.
- The compiler could crash with an assertion failure in `is_nominal_sub_structural` when the type parameter was passed on through an intermediate trait.
- The compiler could hang on an object literal inside a generic method when the object literal provided a trait whose generic method had a type parameter with the same name as the enclosing method's.

In tools built on the `pony_compiler` library, some type parameters now have generated names that start with `$m$`: those of methods inherited from a trait or interface, those of methods and lambdas declared inside such a method, and the copies of these type parameters that an object literal written inside such a method gets. `AST.nice_name()` returns the name written in the source.

Other problems with generic methods are also fixed:

- An `iftype` condition or specialization guard in a trait's generic default method that mentions the method's own type parameter, such as `iftype B <: Comparable[B] val`, was rejected with `type argument is outside its constraint` in every type that inherited the method.
- A type whose generic method has a specialization could be rejected as a subtype of an interface with the same method group. This happened when the type inherited the method group from a trait or interface, or when the guard tested a union (`iftype A <: (U32 val | U16 val)`), an interface such as `Stringable val`, a type that names another of the method's type parameters (`iftype A <: ReadSeq[B] val`), or a type that names the interface's own type parameter (`iftype A <: Array[X] val`).
- Two specializations in one method group that guard different type parameters under the same name, such as `f[A, B] iftype B <: U32 val` and `f[B, A] iftype B <: U32 val`, were reported as unreachable. A specialization made unreachable by an earlier one whose type parameter has a different name was not reported, and in some programs crashed the compiler; it is now reported as unreachable.
- A type could be rejected as a subtype of an interface when its specialization guard had the same `and` or `or` clauses as the interface's in a different order, such as `iftype B <: U8 val and A <: U32 val` against `iftype A <: U32 val and B <: U8 val`.
- A type could be accepted as a subtype of a trait or interface when its specialization's guard tested the same type as the trait's or interface's, such as `U32 val`, but on a different type parameter. A call through the trait or interface could then run the type's default method where the trait's or interface's specialization applied, or the type's specialization where it didn't. Guards on a method's type parameters are now matched by position, so such a type is rejected. For example, with this interface:

  ```pony
  interface I
    fun f[A: Any val, B: Any val](a: A, b: B): U8
    fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: U32 val
  ```

  this class is no longer an `I`, because its guard tests its second type parameter:

  ```pony
  class C
    fun f[B: Any val, A: Any val](a: B, b: A): U8 => 0
    fun f[B: Any val, A: Any val](a: B, b: A): U8 iftype A <: U32 val => 1
  ```

  Guard the same type parameter as the interface, here `iftype B <: U32 val`. The same applies when a specialization guards one of the type's own type parameters where the trait or interface guards one of the method's: such a type is now rejected too.
