## Add Pony Language Server to the Ponyc distribution

We've moved the Pony Language Server that previously was distributed as it's own project to the standard ponyc distribution. This means that the language server will track changes in ponyc. Going forward, the language server that ships with a given ponyc will be fully up-to-date with the compiler and most importantly, it's version of the standard library.

## Handle Bool with exhaustive match

Previously, this function would fail to compile, as true and false were not seen as exhaustive, resulting in the match block exiting and the function attempting to return None.

```pony
  fun fourty_two(err: Bool): USize =>
    match err
    | true => return 50
    | false => return 42
    end
```

We have modified the compiler to recognize that if we have clauses for both true and false, that the match is exhaustive.

Consequently, you can expect the following to now fail to compile with unreachable code:

```pony
  fun forty_three(err: Bool): USize =>
    match err
    | true => return 50
    | false => return 43
    else
      return 44 // Unreachable
    end
```

## Fix ponyc crash when partially applying constructors

We've fixed a compilation assertion error that would result in a compiler crash when utilizing partial constructors.

Previously, this would fail to compile:

```pony
class A
  let n: USize

  new create(n': USize) =>
    n = n'

actor Main
  new create(env: Env) =>
    let ctor = A~create(2)
    let a: A = ctor()
```

## Add support for complex type formatting in LSP hover

The Pony Language Server now properly formats complex types in hover information. Previously, hovering over variables with union types, tuple types, intersection types, or arrow types would display internal token names like `TK_UNIONTYPE` instead of the actual type signature.

Now, the language server correctly displays formatted types such as:
- Union types: `(String | U32 | None)`
- Tuple types: `(String, U32, Bool)`
- Intersection types: `(ReadSeq[U8] & Hashable)`
- Arrow types: function signatures

## Add hover support for generic types in LSP

The LSP server now provides hover information for generic types, including type parameters, type arguments, and capabilities.

Hover displays:
- Type parameters on classes and traits (e.g., `class Container[T: Any val]`)
- Type parameters on methods (e.g., `fun map[U: Any val](f: {(T): U} val): U`)
- Type arguments in instantiated types (e.g., `let items: Array[String val] ref`)
- Capabilities on all types (val, ref, box, iso, trn, tag)
- Nested generic types (e.g., `Array[Array[String val] ref] ref`)
- Viewpoint adaptation/arrow types (e.g., `fun compare(that: box->T): I32`)
- Constructor calls with type arguments (e.g., `GenericPair[String, U32](...)`)

When hovering on variable declarations, the full inferred type is now shown including all generic type arguments and capabilities, making it easier to understand the exact types in your code.

## Add extra hover support to LSP

The Pony Language Server Protocol (LSP) implementation now provides improved hover capabilities with the following fixes.

### Hover on field usages

When hovering over a field reference like `field_name` in the expression `field_name.size()`, the LSP now displays:

```pony
let field_name: String val
```

```pony
class Example
  let field_name: String

  fun get_length(): USize =>
    field_name.size()  // Hovering over 'field_name' here shows: let field_name: String val
```

Previously, hovering over field usages did not show any information.

### Hover on local variable usages

When hovering over a local variable reference like `count` in the expression `count + 1`, the LSP now displays:

```pony
var count: U32 val
```

```pony
fun increment(): U32 =>
  var count: U32 = 0
  count = count + 1  // Hovering over 'count' here shows: var count: U32 val
  count
```

Previously, hovering over variable usages did not show type information.

### Hover on parameter usages

When hovering over a parameter reference like `name` in the expression `name.size()`, the LSP now displays:

```pony
param name: String val
```

```pony
fun get_name_length(name: String): USize =>
  name.size()  // Hovering over 'name' here shows: param name: String val
```

Previously, hovering over parameter usages did not show any information.

### Hover on parameter declarations

When hovering over a parameter declaration like `x: U32` in a function signature, the LSP now displays:

```pony
param x: U32 val
```

```pony
fun calculate(x: U32, y: U32): U32 =>
  // Hovering over 'x' in the signature shows: param x: U32 val
  x + y
```

Previously, hovering over parameter declarations provided no information.

### Hover on method calls

When hovering over a method name in a call expression like `my_object.get_value(42)`, the LSP now correctly highlights only the method name and displays:

```pony
fun get_value(x: U32): String val
```

```pony
class MyObject
  fun get_value(x: U32): String val =>
    x.string()

actor Main
  new create(env: Env) =>
    let my_object = MyObject
    let result = my_object.get_value(42)  // Hovering over 'get_value' highlights only the method name
```

Previously, both the method name and receiver name were highlighted.

## Fix crash when using bare integer literals in array match patterns

Array does not implement the Equatable interface, which queries for structural equality.  Previous to this fix, an error in the type inference logic resulted in the code below resulting in a compiler crash instead of the expected helpful error message:

```pony
actor Main
  new create(env: Env) =>
    let arr: Array[U8 val] = [4; 5]
    match arr
    | [2; 3] => None
    else
      None
    end
```

Now, the correct helpful error message is provided:

```quote
Error:
main.pony:5:7: couldn't find 'eq' in 'Array'
    | [2; 3] => None
      ^
Error:
main.pony:5:7: this pattern element doesn't support structural equality
    | [2; 3] => None
      ^
```

## Add support for go to definition for arrow types in LSP

The LSP server now correctly handles "go to definition" for method calls on arrow types (viewpoint-adapted types like `this->Type`). 

Previously, go-to-definition would fail on method calls where the receiver had an arrow type, such as calling methods on `this` (which has type `this->MyClass` rather than just `MyClass`).

## Add hover support for receiver capability to LSP

The Language Server Protocol (LSP) hover feature now displays receiver capabilities in method signatures.

When hovering over a method reference, the hover information will now show the receiver capability (e.g., `box`, `val`, `ref`, `iso`, `trn`, `tag`) in the method signature. This provides more complete type information at a glance.

### Examples

Previously, hovering over a method would show:

```pony
fun boxed_method(): String
```

Now, it correctly displays:

```pony
fun box boxed_method(): String
```

This applies to all receiver capabilities:

- `fun box method()` - read-only access
- `fun ref method()` - mutable reference access  
- `fun val method()` - immutable value access
- `fun iso method()` - isolated reference access
- `fun trn method()` - transition reference access
- `fun tag method()` - opaque reference access

