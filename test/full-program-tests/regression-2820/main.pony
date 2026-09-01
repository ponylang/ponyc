// Regression test for https://github.com/ponylang/ponyc/issues/2820
//
// A generic class with a constrained type parameter that passes a lambda with
// inferred parameter types to a generic method like Iter.map. The lambda's
// inferred parameter type references the enclosing class's type parameter, but
// the lifted lambda class has its own copy. If the compiler compares these by
// identity rather than equivalence, the type parameter goes unrecognized and
// compilation fails with "the type parameter has no lower bounds."

use "collections"
use "itertools"

class Outer[Info: Any #share]
  let _data: Map[String, Map[String, Info]] =
    Map[String, Map[String, Info]]

  fun test() =>
    Iter[(String, Map[String, Info] box!)](_data.pairs())
      .map[Iter[(String, Info)]]({
        (k_m) =>
          Iter[(String, Info)](k_m._2.pairs())
      })

actor Main
  new create(env: Env) =>
    None
