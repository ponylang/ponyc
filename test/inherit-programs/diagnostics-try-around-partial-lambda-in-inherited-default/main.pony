// Foo.f wraps a call to a partial lambda in try. The lambda calls g through a
// Foo box receiver, so it stays partial even though C's g is not. The program
// must compile and print 7.
trait Foo
  fun g(): U8 ?
  fun f(): U8 =>
    let me: Foo box = this
    let h = {()(me): U8 ? => me.g()?}
    try h()? else 0 end
class C is Foo
  new create() => None
  fun g(): U8 => 7
actor Main
  new create(env: Env) => env.out.print(C.f().string())
