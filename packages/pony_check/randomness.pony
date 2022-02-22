use "random"

class ref Randomness
  """
  Source of randomness, providing methods for generatic uniformly distributed
  values from a given closed interval: [min, max]
  in order for the user to be able to generate every possible value for a given
  primitive numeric type.

  All primitive number method create numbers in range [min, max)
  """
  let _random: Random

  new ref create(seed1: U64 = 42, seed2: U64 = 0) =>
    _random = Rand(seed1, seed2)

  fun ref u8(min: U8 = U8.min_value(), max: U8 = U8.max_value()): U8 =>
    """
    generates a U8 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    if (min == U8.min_value()) and (max == U8.max_value()) then
      _random.u8()
    else
      min + _random.int((max - min).u64() + 1).u8()
    end

  fun ref u16(min: U16 = U16.min_value(), max: U16 = U16.max_value()): U16 =>
    """
    generates a U16 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    if (min == U16.min_value()) and (max == U16.max_value()) then
      _random.u16()
    else
      min + _random.int((max - min).u64() + 1).u16()
    end

  fun ref u32(min: U32 = U32.min_value(), max: U32 = U32.max_value()): U32 =>
    """
    generates a U32 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    if (min == U32.min_value()) and (max == U32.max_value()) then
      _random.u32()
    else
      min + _random.int((max - min).u64() + 1).u32()
    end

  fun ref u64(min: U64 = U64.min_value(), max: U64 = U64.max_value()): U64 =>
    """
    generates a U64 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    if (min == U64.min_value()) and (max == U64.max_value()) then
      _random.u64()
    elseif min > U32.max_value().u64() then
      (u32((min >> 32).u32(), (max >> 32).u32()).u64() << 32) or _random.u32().u64()
    elseif max > U32.max_value().u64() then
      let high = (u32((min >> 32).u32(), (max >> 32).u32()).u64() << 32).u64()
      let low =
        if high > 0 then
          _random.u32().u64()
        else
          u32(min.u32(), U32.max_value()).u64()
        end
      high or low
    else
      // range within U32 range
      u32(min.u32(), max.u32()).u64()
    end

  fun ref u128(
    min: U128 = U128.min_value(),
    max: U128 = U128.max_value())
    : U128
  =>
    """
    generates a U128 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    if (min == U128.min_value()) and (max == U128.max_value()) then
      _random.u128()
    elseif min > U64.max_value().u128() then
      // both above U64 range - chose random low 64 bits
      (u64((min >> 64).u64(), (max >> 64).u64()).u128() << 64) or u64().u128()
    elseif max > U64.max_value().u128() then
      // min below U64 max value
      let high = (u64((min >> 64).u64(), (max >> 64).u64()).u128() << 64)
      let low =
        if high > 0 then
          // number will be bigger than U64 max anyway, so chose a random lower u64
          u64().u128()
        else
          // number <= U64 max, so chose lower u64 while considering requested range min
          u64(min.u64(), U64.max_value()).u128()
        end
      high or low
    else
      // range within u64 range
      u64(min.u64(), max.u64()).u128()
    end

  fun ref ulong(
    min: ULong = ULong.min_value(),
    max: ULong = ULong.max_value())
    : ULong
  =>
    """
    generates a ULong in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    u64(min.u64(), max.u64()).ulong()

  fun ref usize(
    min: USize = USize.min_value(),
    max: USize = USize.max_value())
    : USize
  =>
    """
    generates a USize in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    u64(min.u64(), max.u64()).usize()

  fun ref i8(min: I8 = I8.min_value(), max: I8 = I8.max_value()): I8 =>
    """
    generates a I8 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    min + u8(0, (max - min).u8()).i8()

  fun ref i16(min: I16 = I16.min_value(), max: I16 = I16.max_value()): I16 =>
    """
    generates a I16 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    min + u16(0, (max - min).u16()).i16()

  fun ref i32(min: I32 = I32.min_value(), max: I32 = I32.max_value()): I32 =>
    """
    generates a I32 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    min + u32(0, (max - min).u32()).i32()

  fun ref i64(min: I64 = I64.min_value(), max: I64 = I64.max_value()): I64 =>
    """
    generates a I64 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    min + u64(0, (max - min).u64()).i64()


  fun ref i128(
    min: I128 = I128.min_value(),
    max: I128 = I128.max_value())
    : I128
  =>
    """
    generates a I128 in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    min + u128(0, (max - min).u128()).i128()


  fun ref ilong(
    min: ILong = ILong.min_value(),
    max: ILong = ILong.max_value())
    : ILong
  =>
    """
    generates a ILong in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    min + ulong(0, (max - min).ulong()).ilong()

  fun ref isize(
    min: ISize = ISize.min_value(),
    max: ISize = ISize.max_value())
    : ISize
  =>
    """
    generates a ISize in closed interval [min, max]
    (default: [min_value, max_value])

    behavior is undefined if min > max.
    """
    min + usize(0, (max - min).usize()).isize()


  fun ref f32(min: F32 = 0.0, max: F32 = 1.0): F32 =>
    """
    generates a F32 in closed interval [min, max]
    (default: [0.0, 1.0])
    """
    (_random.real().f32() * (max-min)) + min


  fun ref f64(min: F64 = 0.0, max: F64 = 1.0): F64 =>
    """
    generates a F64 in closed interval [min, max]
    (default: [0.0, 1.0])
    """
    (_random.real() * (max-min)) + min

  fun ref bool(): Bool =>
    """
    generates a random Bool value
    """
    (_random.next() % 2) == 0

  fun ref shuffle[T](array: Array[T] ref) =>
    _random.shuffle[T](array)


