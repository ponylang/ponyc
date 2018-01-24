"""
This code triggered a ponyc segmentation fault when using LLVM versions
prior to 5.0.1. The crash only took place when building optimised binaries.
i.e. ponyc -d did not tigger the crash.

It is included here in order to protect againt regressions.

See: https://github.com/ponylang/ponyc/issues/2280
"""

primitive GT
	fun arb(): Bool => true
primitive LT
	fun arb(): Bool => true

type Comp is (LT | GT)

type Tuple is (I32, I32)

actor LiteralOptimisation

	new create() =>
		let b: Tuple = if select((5,0),(2,0)) is LT then
			(0,0)
		else
			(0,1)
		end
		let c: Comp = select((5,0), b)
		c.arb()

	fun select(lhs: Tuple, rhs: Tuple): Comp =>
		if lhs._1 < rhs._1 then
			LT
		else
			GT
		end

actor Main
	new create(env: Env) =>
		let worker: LiteralOptimisation = LiteralOptimisation
