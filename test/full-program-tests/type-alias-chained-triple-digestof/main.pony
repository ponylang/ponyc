// Regression test for 3-level chained type aliases in digestof.
//
// The other chained-alias tests in this PR use 2-level chains
// (`type _Middle is _Pair; type _Pair is (U64, U64)`). This test
// verifies that the transitive unfold in `typealias_unfold` is correct
// for a deeper chain: `_Outermost -> _Middle -> _Pair -> (U64, U64)`.
//
// The recursive algorithm is correct by induction, so this test adds
// positive coverage for N=3 rather than a distinct crash site. It would
// catch a hypothetical future regression where the transitive recursion
// stopped after N=2 or had a depth bug. See ponylang/ponyc#5195.

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair
type _Outermost is _Middle

actor Main
  new create(env: Env) =>
    let aliased: _Outermost = (U64(1), U64(2))
    let plain: (U64, U64) = (U64(1), U64(2))
    if (digestof aliased) == (digestof plain) then
      @pony_exitcode(1)
    end
