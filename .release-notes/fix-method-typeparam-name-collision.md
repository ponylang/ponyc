## Fix inherited method using receiver's type argument for same-named method type parameter

When a type inherited a generic trait method and one of the type's own type parameters had the same name as a type parameter on the method, the compiler substituted the type's argument for the method's parameter. `Combined[None].check[Label](None, Label)` resolved `Label` to `None` because `Combined` and `check` both named a parameter `B`.

```pony
trait Group[A: Any val]
  fun check[B: Labeled](arg: A, item: B): U64 => item.label()

primitive Combined[B: Any val] is Group[B]
```

Calling `Combined[None].check[Label](None, Label)` failed to compile or produced wrong results because the compiler treated the method's `B` as the type's `B`.

Renaming one of the parameters is no longer required.
