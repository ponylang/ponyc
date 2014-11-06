class MT is Random
  var _state: Array[U64]
  var _index: U64

  new create(seed: U64 = 5489) =>
    _state = Array[U64].prealloc(_n())
    _index = _n()

    var x = seed

    try
      _state.append(x)
      var i: U64 = 1

      while i < _n() do
        x = ((x xor (x >> 62)) * 6364136223846793005) + i
        _state.append(x)
        i = i + 1
      end
    end

  // A random integer in [0, 2^64 - 1]
  fun ref next(): U64 =>
    if _index >= _n() then
      _populate()
    end

    var x = try _state(_index) else U64(0) end //TODO: access is safe
    _index = _index + 1

    x = x xor ((x >> 29) and 0x5555555555555555)
    x = x xor ((x << 17) and 0x71d67fffeda60000)
    x = x xor ((x << 37) and 0xfff7eee000000000)
    x xor (x >> 43)

  fun ref _populate() =>
    try
      _index = 0
      var x = _state(0)
      var i: U64 = 0

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

  fun tag _n(): U64 => 312
  fun tag _m(): U64 => 156
  fun tag _n1(): U64 => _n() - 1

  fun tag _mask(x: U64, y: U64): U64 =>
    (x and 0xffffffff80000000) or (y and 0x000000007fffffff)

  fun tag _matrix(x: U64): U64 => (x and 1) * 0xb5026f5aa96619e9

  fun tag _mix(x: U64, y: U64): U64 =>
    let z = _mask(x, y)
    (z >> 1) xor _matrix(z)

  fun ref _lower(i: U64, x: U64): U64 ? =>
    let y = _state(i + 1)
    _state(i) = _state(i + _m()) xor _mix(x, y)
    y

  fun ref _upper(i: U64, x: U64): U64 ? =>
    let y = _state(i + 1)
    _state(i) = _state(i - _m()) xor _mix(x, y)
    y
