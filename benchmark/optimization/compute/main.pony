"""
Raw numeric computation with no Pony object allocation.

Evaluates a polynomial in a tight loop. Exercises LLVM's scalar optimization
pipeline (strength reduction, loop optimization, constant folding) without
involving HeapToStack or other Pony-specific passes. A regression here
indicates ThinLTO's standard optimization is weaker than the old monolithic O3.
"""

actor Main
  new create(env: Env) =>
    var sum: F64 = 0
    var i: U64 = 0
    while i < 500_000_000 do
      let x = i.f64() * 1e-9
      sum = sum + (((x * x) - (x * 0.5)) + 0.25)
      i = i + 1
    end
    env.out.print(sum.string())
