// C gives D ref for A, and f's iftype asks for A <: Named #read. The branch
// that runs is chosen with capabilities compared exactly, so for D ref the
// then branch runs, and its object literal's class must be generated. The
// test expects "then".
interface Named
  fun name(): String

trait Foo[A: Any #read]
  fun f(a: A): String =>
    iftype A <: Named #read then
      let s: Stringable box = object is Stringable
        let _a: A = a
        fun string(): String iso^ => "then".clone()
      end
      s.string()
    else
      "else"
    end

class D is Named
  new create() => None
  fun name(): String => "D"

class C is Foo[D ref]
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.f(D))
