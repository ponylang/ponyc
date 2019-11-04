class XorOshiro128Plus is Random
  """
  This is an implementation of xoroshiro128+, as detailed at:

  http://xoroshiro.di.unimi.it

  This is currently the default Rand implementation.
  """
  // state
  var _x: U64
  var _y: U64

  new from_u64(x: U64 = 5489) =>
    """
    Use seed x to seed a [SplitMix64](random-SplitMix64.md) and use this to
    initialize the 128 bits of state.
    """
    let sm = SplitMix64(x)
    _x = sm.next()
    _y = sm.next()

  new create(x: U64 = 5489, y: U64 = 0) =>
    """
    Create with the specified seed. Returned values are deterministic for a
    given seed.
    """
    _x = x
    _y = y
    next()

  fun ref next(): U64 =>
    """
    A random integer in [0, 2^64)
    """
    let x = _x
    var y = _y
    let r = x + y

    y = x xor y
    _x = x.rotl(24) xor y xor (y << 16)
    _y = y.rotl(37)
    r

class XorOshiro128StarStar is Random
  """
  This is an implementation of xoroshiro128**, as detailed at:

  http://xoshiro.di.unimi.it/

  This Rand implementation is slightly slower than [XorOshiro128Plus](random-XorOshiro128Plus.md)
  but does not exhibit "mild dependencies in Hamming weights" (the lower four bits might fail linearity tests).
  """

  var _x: U64
  var _y: U64

  new from_u64(x: U64 = 5489) =>
    """
    Use seed x to seed a [SplitMix64](random-SplitMix64.md) and use this to
    initialize the 128 bits of state.
    """
    let sm = SplitMix64(x)
    _x = sm.next()
    _y = sm.next()

  new create(x: U64 = 5489, y: U64 = 0) =>
    _x = x
    _y = y
    next()

  fun ref next(): U64 =>
    let x = _x
    var y = _y
    let r = (x * 5).rotl(7) * 9
    y = x xor y

    _x = x.rotl(24) xor y xor (y << 16)
    _y = y.rotl(37)
    r
