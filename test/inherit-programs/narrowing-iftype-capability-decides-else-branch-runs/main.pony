// C gives P val for A, and f's iftype asks for A <: Named box. The branch
// that runs is chosen with capabilities compared exactly, so for P val the
// else branch runs, and its object literal's class must be generated. The
// test expects "else".
interface box Named
  fun name(): String
trait Foo[A: Any val]
  fun f(a: A): String =>
    iftype A <: Named box then "then"
    else
      let s: Stringable box = object is Stringable
        let _a: A = a
        fun string(): String iso^ => "else".clone()
      end
      s.string()
    end
primitive P is Named
  fun name(): String => "P"
class C is Foo[P]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(P))
