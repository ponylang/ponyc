class A
  var v: Any val
  new create(v': Any val) =>
    v = consume v'

class B

actor Foo
  var _x: (A iso | B val | None)

  new create(x': (A iso | B val | None)) =>
    _x = consume x'

  be f() =>
    match (_x = None)
    | let a: A iso => None
    end

actor Main
  new create(env: Env) =>
    let f = Foo(recover A("hello".string()) end)
    f.f()
