// Foo.f is partial and calls g?. C overrides g with a version that cannot
// raise an error. C's inherited f must compile and print 7.
trait Foo
  fun g(): U8 ?
  fun f(): U8 ? => g()?
class C is Foo
  new create() => None
  fun g(): U8 => 7
actor Main
  new create(env: Env) => try env.out.print(C.f()?.string()) end
