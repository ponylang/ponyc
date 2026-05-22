// End-to-end test for a recursive type alias whose recursion lives
// at the alias level: _Tree references itself through Array's typearg,
// so _Tree's alias-graph node has a self-loop classified as a
// constructive cycle by PASS_TYPEALIAS_RECURSION.
//
// Builds a small tree of integer leaves and array-typed branches,
// walks it at runtime, and uses the walked total as the exit code.
// Exercises codegen, reach, gentrace, and runtime tracing on a value
// typed by a recursive alias (the libponyc.tests fixture only checks
// IR).

use @pony_exitcode[None](code: I32)

type _Tree is (_Leaf | Array[_Tree])

class _Leaf
  let value: I32
  new create(v: I32) =>
    value = v

actor Main
  new create(env: Env) =>
    // Tree:                Array
    //                     /  |  \
    //                Array  Leaf(4) Leaf(8)
    //                /  \
    //          Leaf(1)   Leaf(2)
    let inner: Array[_Tree] = Array[_Tree]
    inner.push(_Leaf(1))
    inner.push(_Leaf(2))

    let outer: Array[_Tree] = Array[_Tree]
    outer.push(inner)
    outer.push(_Leaf(4))
    outer.push(_Leaf(8))

    let t: _Tree = outer
    @pony_exitcode(_sum(t))

  fun _sum(t: _Tree): I32 =>
    match t
    | let l: _Leaf => l.value
    | let arr: Array[_Tree] =>
      var total: I32 = 0
      for child in arr.values() do
        total = total + _sum(child)
      end
      total
    end
