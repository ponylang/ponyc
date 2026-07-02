// Regression test for ponylang/ponyc#2701 and ponylang/ponyc#2790.
//
// Mirrors `array-infer-from-values-interface-type-alias-element` with a
// locally-declared interface and type alias so the regression is pinned
// to the shape that triggered the crash, not to the stdlib definitions
// of `ByteSeqIter`/`ByteSeq`. The shape: an interface whose `values()`
// method returns `Iterator[this->Alias box]` where `Alias` is a type
// alias that expands into a union. Element-type inference for array
// literals with this antecedent must recursively strip `this->` arrows
// from inside the alias-expanded union before the type reaches code
// generation.

type _Elem is (String | Array[U8] val)

interface val _Elems
  fun values(): Iterator[this->_Elem box]

primitive _Sink
  fun apply(iter: _Elems) => None

actor Main
  new create(env: Env) =>
    _Sink([])
    _Sink(["foo"; "bar"])
    _Sink(["foo"; "bar".array(); "baz"])
