class SplitMix64 is Random
  """
  Very fast Pseudo-Random-Number-Generator
  using only 64 bit of state, as detailed at:

  http://xoshiro.di.unimi.it/ and http://gee.cs.oswego.edu/dl/papers/oopsla14.pdf

  Using [XorOshiro128StarStar](random-XorOshiro128StarStar.md) or [XorOshiro128Plus](random-XorOshiro128Plus.md)
  should be prefered unless using only 64 bit of state is a requirement.
  """
  // state
  var _x: U64

  new from_64_bits(x: U64 = 5489) =>
    _x = x

  new create(x: U64 = 5489, y: U64 = 0) =>
    """
    Only x is used, y is discarded.
    """
    _x = x

  fun ref next(): U64 =>
    _x = _x + U64(0x9e3779b97f4a7c15)
    var z: U64 = _x
    z = (z xor (z >> 30)) * U64(0xbf58476d1ce4e5b9)
    z = (z xor (z >> 27)) * U64(0x94d049bb133111eb)
    z xor (z >> 31)

