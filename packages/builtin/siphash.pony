
class ref SipHash24Streaming
  """
  Provides SipHash24 for non-array data
  given as separate `U64`s.

  ### Usage:

  ```pony
  use "collections"

  actor Main
    new create(env: Env) =>
      try
        let sip = SipHash24Streaming.create()
        for x in Range[U64](1, 100).values() do
          // feed consecutive U64s to be hashed
          sip.update(x)
        end
        // execute finishing steps, reset internal state so this instance can be
        // reused, and output the computed hash
        let hash = sip.finish()
        env.out.print("HASHED: " + hash.string())
      end
  ```

  """
  var _v0: U64 = 0
  var _v1: U64 = 0
  var _v2: U64 = 0
  var _v3: U64 = 0

  var _size: USize = 0

  new ref create() =>
    reset()

  fun ref reset() =>
    """
    Reset the internal state.
    """
    _v0 = SipHash24._k0() xor 0x736f6d6570736575
    _v1 = SipHash24._k1() xor 0x646f72616e646f6d
    _v2 = SipHash24._k0() xor 0x6c7967656e657261
    _v3 = SipHash24._k1() xor 0x7465646279746573
    _size = 0

  fun ref update(m: U64) =>
    """
    Hash the given `m` and update the internal state accordingly.
    """
    _v3 = _v3 xor m
    (_v0, _v1, _v2, _v3) = SipHash24._sipround64(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = SipHash24._sipround64(_v0, _v1, _v2, _v3)
    _v0 = _v0 xor m
    _size = _size + 8

  fun ref finish(): U64 =>
    """
    This method finally computes the hash from all the data added with `update`,
    and resets the internal state,
    so this instance can be conveniently reused for another hash calculation.
    """
    let b = (_size << USize(56)).u64()
    _v3 = _v3 xor b
    (_v0, _v1, _v2, _v3) = SipHash24._sipround64(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = SipHash24._sipround64(_v0, _v1, _v2, _v3)
    _v0 = _v0 xor b
    _v2 = _v2 xor 0xFF
    (_v0, _v1, _v2, _v3) = SipHash24._sipround64(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = SipHash24._sipround64(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = SipHash24._sipround64(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = SipHash24._sipround64(_v0, _v1, _v2, _v3)

    let result = _v0 xor _v1 xor _v2 xor _v3
    reset() // BEWARE
    result

primitive SipHash24

  fun _k0(): U64 => U64(0x8A109C6B22D309FE)
  fun _k1(): U64 => U64(0x9F923FCCB57235E1)

  fun _sipround64(v0: U64, v1: U64, v2: U64, v3: U64): (U64, U64, U64, U64) =>
    var t0 = v0 + v1
    var t1 = v1.rotl(13)
    t1 = t1 xor t0
    t0 = t0.rotl(32)
    var t2 = v2 + v3
    var t3 = v3.rotl(16)
    t3 = t3 xor t2
    t0 = t0 + t3
    t3 = t3.rotl(21)
    t3 = t3 xor t0
    t2 = t2 + t1
    t1 = t1.rotl(17)
    t1 = t1 xor t2
    t2 = t2.rotl(32)


    (t0, t1, t2, t3)

  fun apply[T: ReadSeq[U8] #read](data: T): U64 =>
    let size = data.size()
    var b: U64  = (size << USize(56)).u64()

    var v0 = _k0() xor 0x736f6d6570736575
    var v1 = _k1() xor 0x646f72616e646f6d
    var v2 = _k0() xor 0x6c7967656e657261
    var v3 = _k1() xor 0x7465646279746573

    let endi: USize = size - (size % 8)

    try
      var i = USize(0)
      while i < endi do
        let m: U64 =
          iftype T <: ReadAsNumerics then
            data.read_u64(i)?
          elseif T <: String val then
            data.array().read_u64(i)?
          else
            (data(i)?.u64()) or
            (data(i + 1)?.u64() << 8) or
            (data(i + 2)?.u64() << 16) or
            (data(i + 3)?.u64() << 24) or
            (data(i + 4)?.u64() << 32) or
            (data(i + 5)?.u64() << 40) or
            (data(i + 6)?.u64() << 48) or
            (data(i + 7)?.u64() << 56)
          end
        v3 = v3 xor m
        (v0, v1, v2, v3) = _sipround64(v0, v1, v2, v3)
        (v0, v1, v2, v3) = _sipround64(v0, v1, v2, v3)
        v0 = v0 xor m

        i = i + 8
      end

      // bad emulation of a C switch statement with  fallthrough
      let rest = size and 7
      if rest >= 1 then
        if rest >= 2 then
          if rest >= 3 then
            if rest >= 4 then
              if rest >= 5 then
                if rest >= 6 then
                  if rest == 7 then
                    b = b or (data(endi + 6)?.u64() << 48)
                  end
                  b = b or (data(endi + 5)?.u64() << 40)
                end
                b = b or (data(endi + 4)?.u64() << 32)
              end
              b = b or (data(endi + 3)?.u64() << 24)
            end
            b = b or (data(endi + 2)?.u64() << 16)
          end
          b = b or (data(endi + 1)?.u64() << 8)
        end
        b = b or data(endi)?.u64()
      end

      v3 = v3 xor b
      (v0, v1, v2, v3) = _sipround64(v0, v1, v2, v3)
      (v0, v1, v2, v3) = _sipround64(v0, v1, v2, v3)
      v0 = v0 xor b
      v2 = v2 xor 0xFF
      (v0, v1, v2, v3) = _sipround64(v0, v1, v2, v3)
      (v0, v1, v2, v3) = _sipround64(v0, v1, v2, v3)
      (v0, v1, v2, v3) = _sipround64(v0, v1, v2, v3)
      (v0, v1, v2, v3) = _sipround64(v0, v1, v2, v3)
      v0 xor v1 xor v2 xor v3
    else
      // should never happen, but we can't prove it to the compiler...
      -1
    end


class ref HalfSipHash24Streaming
  var _v0: U32 = 0
  var _v1: U32 = 0
  var _v2: U32 = 0
  var _v3: U32 = 0

  var _size: USize = 0

  new ref create() =>
    reset()

  fun ref reset() =>
    """
    Reset the internal state.
    """
    _v0 = HalfSipHash24._k0() xor 0x736f6d65
    _v1 = HalfSipHash24._k1() xor 0x646f7261
    _v2 = HalfSipHash24._k0() xor 0x6c796765
    _v3 = HalfSipHash24._k1() xor 0x74656462
    _size = 0

  fun ref update(m: U32) =>
    _v3 = _v3 xor m
    (_v0, _v1, _v2, _v3) = HalfSipHash24._sipround32(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = HalfSipHash24._sipround32(_v0, _v1, _v2, _v3)
    _v0 = _v0 xor m
    _size = _size + 4

  fun ref finish(): U32 =>
    let b  = (_size << USize(24)).u32()
    _v3 = _v3 xor b
    (_v0, _v1, _v2, _v3) = HalfSipHash24._sipround32(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = HalfSipHash24._sipround32(_v0, _v1, _v2, _v3)
    _v0 = _v0 xor b
    _v2 = _v2 xor 0xFF
    (_v0, _v1, _v2, _v3) = HalfSipHash24._sipround32(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = HalfSipHash24._sipround32(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = HalfSipHash24._sipround32(_v0, _v1, _v2, _v3)
    (_v0, _v1, _v2, _v3) = HalfSipHash24._sipround32(_v0, _v1, _v2, _v3)
    let result = _v0 xor _v1 xor _v2 xor _v3
    reset()
    result

primitive HalfSipHash24

  fun _k0(): U32 => U32(0x22D309FE)
  fun _k1(): U32 => U32(0x8A109C6B)

  fun _sipround32(v0: U32, v1: U32, v2: U32, v3: U32): (U32, U32, U32, U32) =>
    var t0 = v0 + v1
    var t1 = v1.rotl(5)
    t1 = t1 xor t0
    t0 = t0.rotl(16)
    var t2 = v2 + v3
    var t3 = v3.rotl(8)
    t3 = t3 xor t2
    t0 = t0 + t3
    t3 = t3.rotl(7)
    t3 = t3 xor t0
    t2 = t2 + t1
    t1 = t1.rotl(13)
    t1 = t1 xor t2
    t2 = t2.rotl(16)

    (t0, t1, t2, t3)

  fun apply[T: ReadSeq[U8] #read](data: T): U32 =>

    let size = data.size()
    var b: U32  = (size << USize(24)).u32()

    var v0 = _k0() xor 0x736f6d65
    var v1 = _k1() xor 0x646f7261
    var v2 = _k0() xor 0x6c796765
    var v3 = _k1() xor 0x74656462

    let endi: USize = size - (size % 4)

    try
      var i = USize(0)
      while i < endi do
        let m: U32 =
          iftype T <: ReadAsNumerics then
            data.read_u32(i)?
          elseif T <: String val then
            data.array().read_u32(i)?
          else
            (data(i)?.u32()) or
            (data(i + 1)?.u32() << 8) or
            (data(i + 2)?.u32() << 16) or
            (data(i + 3)?.u32() << 24)
          end
        v3 = v3 xor m
        (v0, v1, v2, v3) = _sipround32(v0, v1, v2, v3)
        (v0, v1, v2, v3) = _sipround32(v0, v1, v2, v3)
        v0 = v0 xor m

        i = i + 4
      end

      // bad emulation of a C switch statement with  fallthrough
      let rest = size and 3
      if rest >= 1 then
        if rest >= 2 then
          if rest >= 3 then
            b = b or (data(endi + 2)?.u32() << 16)
          end
          b = b or (data(endi + 1)?.u32() << 8)
        end
        b = b or data(endi)?.u32()
      end

      v3 = v3 xor b
      (v0, v1, v2, v3) = _sipround32(v0, v1, v2, v3)
      (v0, v1, v2, v3) = _sipround32(v0, v1, v2, v3)
      v0 = v0 xor b
      v2 = v2 xor 0xFF
      (v0, v1, v2, v3) = _sipround32(v0, v1, v2, v3)
      (v0, v1, v2, v3) = _sipround32(v0, v1, v2, v3)
      (v0, v1, v2, v3) = _sipround32(v0, v1, v2, v3)
      (v0, v1, v2, v3) = _sipround32(v0, v1, v2, v3)
      v0 xor v1 xor v2 xor v3
    else
      // should never happen, but we can't prove it to the compiler...
      -1
    end

