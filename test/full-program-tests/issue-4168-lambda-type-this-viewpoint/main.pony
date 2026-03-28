"""
Regression test for issue #4168. When this-> appears in a lambda type parameter
(e.g. {(this->A!)}), it must resolve to the enclosing method's receiver cap,
not the desugared interface's receiver. Without the fix, forwarding the lambda
to another function caused wrong vtable dispatch and segfaults.
"""
use @pony_exitcode[None](code: I32)

class Container[A: Any #read]
  let _data: A

  new create(a: A) =>
    _data = a

  fun apply(): this->A =>
    _data

  // this-> in the lambda type is the key construct being tested.
  // It should resolve to box-> since fun defaults to box receiver.
  fun update(f: {(this->A!)}) =>
    Applier[A, this->Container[A]](this, f)

primitive Applier[A: Any #read, B: Container[A] #read]
  fun apply(c: B, f: {(B->A!)}) =>
    f(c.apply())

class Counter
  var value: I32 = 0

actor Main
  new create(env: Env) =>
    let c = Container[Counter](Counter)
    c.update({(x: Counter box) =>
      if x.value == 0 then
        @pony_exitcode(1)
      end
    })
