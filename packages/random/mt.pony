class MT is Random
  """
  A Mersenne Twister. This is a non-cryptographic random number generator.
  """
  var _state: Array[U64]
  var _index: USize

  new create(seed: U64 = 5489) =>
    """
    Create with the specified seed. Returned values are deterministic for a
    given seed.
    """
    _state = Array[U64](_n())
    _index = _n()

    var x = seed

    _state.push(x)
    var i: USize = 1

    while i < _n() do
      x = ((x xor (x >> 62)) * 6364136223846793005) + i.u64()
      _state.push(x)
      i = i + 1
    end

  fun ref next(): U64 =>
    """
    A random integer in [0, 2^64 - 1]
    """
    if _index >= _n() then
      _populate()
    end

    try
      var x = _state(_index)
      _index = _index + 1

      x = x xor ((x >> 29) and 0x5555555555555555)
      x = x xor ((x << 17) and 0x71d67fffeda60000)
      x = x xor ((x << 37) and 0xfff7eee000000000)
      x xor (x >> 43)
    else
      0
    end

  fun ref _populate() =>
    """
    Repopulates the state array.
    """
    try
      _index = 0
      var x = _state(0)
      var i: USize = 0

      while i < _m() do
        x = _lower(i, x)
        i = i + 1
      end

      x = _state(_m())
      i = _m()

      while i < _n1() do
        x = _upper(i, x)
        i = i + 1
      end

      _upper(_n1(), _state(_n1()))
    end

  fun tag _n(): USize => 312
  fun tag _m(): USize => 156
  fun tag _n1(): USize => _n() - 1

  fun tag _mask(x: U64, y: U64): U64 =>
    (x and 0xffffffff80000000) or (y and 0x000000007fffffff)

  fun tag _matrix(x: U64): U64 => (x and 1) * 0xb5026f5aa96619e9

  fun tag _mix(x: U64, y: U64): U64 =>
    let z = _mask(x, y)
    (z >> 1) xor _matrix(z)

  fun ref _lower(i: USize, x: U64): U64 ? =>
    let y = _state(i + 1)
    _state(i) = _state(i + _m()) xor _mix(x, y)
    y

  fun ref _upper(i: USize, x: U64): U64 ? =>
    let y = _state(i + 1)
    _state(i) = _state(i - _m()) xor _mix(x, y)
    y
