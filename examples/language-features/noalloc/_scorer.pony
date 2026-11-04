trait _Scorer
  fun \noalloc\ score(a: U32, b: U32): U32

class _SimpleScorer is _Scorer
  fun \noalloc\ score(a: U32, b: U32): U32 =>
    if a > b then a else b end

class _WeightedScorer is _Scorer
  let _weight: U32

  new create(weight: U32) =>
    _weight = weight

  fun \noalloc\ score(a: U32, b: U32): U32 =>
    if a > b then a * _weight else b * _weight end
