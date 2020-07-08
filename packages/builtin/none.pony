primitive None is (Stringable & Hashable & Hashable64)
  fun string(): String iso^ =>
    "None".string()

  fun hash_with(state': HashState): HashState => state'

  fun hash_with64(state': HashState64): HashState64 => state'
