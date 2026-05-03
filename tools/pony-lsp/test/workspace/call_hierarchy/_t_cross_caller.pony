use "callee"

class _TCrossCaller
  fun cross_call(c: TCallee): None =>
    c.callee_method()
