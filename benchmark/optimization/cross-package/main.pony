"""
Stress test for cross-package HeapToStack promotion.

Allocates Point objects (defined in the lib/ package) in a tight loop and
passes them to Geometry.offset_sum (also in lib/). For HeapToStack to promote
the Points, the inliner must first inline offset_sum and the Point constructor
across the package boundary, making the allocations visible as non-escaping.

With per-package LLVM modules, this requires cross-module inlining — ThinLTO
function importing or full LTO module merging. A single-module build (current
ponyc) inlines everything within one module, so this benchmark establishes the
baseline that per-package builds must match.

200M Point allocations per run. If promoted: ~0.2s. If not: several seconds.
"""
use "lib"

actor Main
  new create(env: Env) =>
    var sum: U64 = 0
    var i: U64 = 0
    while i < 100_000_000 do
      let a = Point(i, i + 1)
      let b = Point(i + 2, i + 3)
      sum = sum + Geometry.offset_sum(a, b)
      i = i + 1
    end
    env.out.print(sum.string())
