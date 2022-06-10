actor Main
  new create(env: Env) =>
    // zero-parameter constructor as argument and assignment
    Bar.take_foo(Foo)
    let qux: Foo iso = Foo

    // sendable-parameter constructor as argument and assignment
    Bar.take_foo(Foo.from_u8(88))
    let bar: Foo iso = Foo.from_u8(88)

    // non-sendable parameter ctor and assignment must fail
    // see RecoverTest.CantAutoRecover_CtorArgWithNonSendableArg
    // and RecoverTest.CantAutoRecover_CtorAssignmentWithNonSendableArg

    //let s: String ref = String
    //Bar.take_foo(Foo.from_str(s)) -> argument not assignable to parameter
    //let baz: Foo iso = Foo.from_str(s) -> right side must be a subtype of left side

    // verify that inheritance hasn't broken
    let x = String
    let y: String ref = x

class Foo
  new create() =>
    None
  new ref from_u8(v: U8) =>
    None
  new ref from_str(s: String ref) =>
    None

primitive Bar
  fun take_foo(foo: Foo iso) =>
    None
