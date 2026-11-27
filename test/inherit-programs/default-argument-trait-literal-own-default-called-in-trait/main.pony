// T.mk makes a literal that provides T and has its own method g with a
// default of 3. Calling o.g() in T.mk must use that default and print 3.
trait T
  fun label(): String => "t"
  fun mk(): String =>
    let o = object is T fun g(x: U8 = 3): U8 => x end
    o.g().string()
class C is T
actor Main
  new create(env: Env) => env.out.print(C.mk())
