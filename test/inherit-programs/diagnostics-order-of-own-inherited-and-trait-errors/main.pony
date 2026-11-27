// lib's Foo.f() and Bar's own g() both return a String where U8 is declared.
// Expects three errors in this order: Bar's g(), Bar's copy of f() with a note
// naming the trait, then Foo's f() itself.
use l = "lib"
class Bar is l.Foo
  fun g(): U8 => "main"
actor Main
  new create(env: Env) => None
