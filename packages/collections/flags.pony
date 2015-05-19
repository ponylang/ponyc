interface Flag[A: (Unsigned & Integer[A] box)] val
  """
  A flag should be a primitive with a value method that returns the bits that
  represent the flag. This allows a flag to encode a single bit, or any
  combination of bits.
  """
  fun value(): A

class Flags[A: Flag[B] val, B: (Unsigned & Integer[B] box) = U64] is
  Ordered[Flags[A, B] box]
  """
  Flags is a set of flags. The flags that are recognised should be passed as
  a union type for type parameter A. For example:

  primitive SSE
    fun value(): U64 => 1

  primitive AVX
    fun value(): U64 => 2

  primitive RDTSCP
    fun value(): U64 => 4

  type Features is Flags[(SSE | AVX | RDTSCP)]
  """
  var _value: B = 0

  fun value(): B =>
    """
    Returns the bit encoding of the set flags.
    """
    _value

  fun apply(flag: A): Bool =>
    """
    Returns true if the flag is set.
    """
    (_value and flag.value()) > 0

  fun ref add(flag: A): Flags[A, B]^ =>
    """
    Sets the flag.
    """
    _value = _value or flag.value()
    this

  fun ref sub(flag: A): Flags[A, B]^ =>
    """
    Unsets the flag.
    """
    _value = _value and not flag.value()
    this

  fun op_or(that: Flags[A, B] box): Flags[A, B] iso^ =>
    """
    The union of this and that.
    """
    let f = recover Flags[A, B] end
    f._value = this._value or that._value
    f

  fun op_and(that: Flags[A, B] box): Flags[A, B] iso^ =>
    """
    The intersection of this and that.
    """
    let f = recover Flags[A, B] end
    f._value = this._value and that._value
    f

  fun op_xor(that: Flags[A, B] box): Flags[A, B] iso^ =>
    """
    The symmetric difference of this and that.
    """
    let f = recover Flags[A, B] end
    f._value = this._value and that._value
    f

  fun op_not(): Flags[A, B] iso^ =>
    """
    The inverse of this.
    """
    let f = recover Flags[A, B] end
    f._value = not this._value
    f

  fun without(that: Flags[A, B] box): Flags[A, B] iso^ =>
    """
    The flags in this that are not in that.
    """
    let f = recover Flags[A, B] end
    f._value = this._value and not that._value
    f

  fun clone(): Flags[A, B] iso^ =>
    """
    Create a clone.
    """
    let f = recover Flags[A, B] end
    f._value = this._value
    f

  fun eq(that: Flags[A, B] box): Bool =>
    """
    Returns true if this has the same flags set as that.
    """
    _value == that._value

  fun lt(that: Flags[A, B] box): Bool =>
    """
    Returns true if the flags set on this are a subset of the flags set on
    that. Flags is an ordered type, so all of the comparison operators work.
    """
    (_value != that._value) and ((_value and not that._value) == 0)
