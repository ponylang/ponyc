class XorOshiro128Plus is Random
  """
  This is an implementation of xoroshiro128+, as detailed at:

  http://xoroshiro.di.unimi.it

  This is currently the default Rand implementation.
  """
  var _x: U64
  var _y: U64

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
    _x = x.rotl(55) xor y xor (y << 14)
    _y = y.rotl(36)
    r
