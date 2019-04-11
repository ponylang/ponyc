class XorShift128Plus is Random
  """
  This is an implementation of xorshift+, as detailed at:

  http://xoroshiro.di.unimi.it

  This should only be used for legacy applications that specifically require
  XorShift128Plus, otherwise use Rand.
  """
  var _x: U64
  var _y: U64

  new from_64bit(x: U64 = 5489) =>
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
    var y = _x
    let x = _y
    let r = x + y
    _x = x
    y = y xor (y << 23)
    _y = y xor x xor (y >> 18) xor (x >> 5)
    r
