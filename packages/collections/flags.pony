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

  Type parameter B is the unlying field used to store the flags.
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

  fun ref all(): Flags[A, B]^ =>
    """
    Sets all bits, including undefined flags.
    """
    _value = -1
    this

  fun ref clear(): Flags[A, B]^ =>
    """
    Unsets all flags.
    """
    _value = 0
    this

  fun ref set(flag: A): Flags[A, B]^ =>
    """
    Sets the flag.
    """
    _value = _value or flag.value()
    this

  fun ref unset(flag: A): Flags[A, B]^ =>
    """
    Unsets the flag.
    """
    _value = _value and not flag.value()
    this

  fun ref flip(flag: A): Flags[A, B]^ =>
    """
    Sets the flag if it is unset, unsets the flag if it is set.
    """
    _value = _value xor flag.value()
    this

  fun ref union(that: Flags[A, B] box): Flags[A, B]^ =>
    """
    The union of this and that.
    """
    _value = this._value or that._value
    this

  fun ref intersect(that: Flags[A, B] box): Flags[A, B]^ =>
    """
    The intersection of this and that.
    """
    _value = this._value and that._value
    this

  fun ref difference(that: Flags[A, B] box): Flags[A, B]^ =>
    """
    The symmetric difference of this and that.
    """
    _value = this._value xor that._value
    this

  fun ref remove(that: Flags[A, B] box): Flags[A, B]^ =>
    """
    Unset flags that are set in that.
    """
    _value = this._value xor that._value
    this

  fun add(flag: A): Flags[A, B] iso^ =>
    """
    This with the flag set.
    """
    let f = recover Flags[A, B] end
    f._value = this._value or flag.value()
    f

  fun sub(flag: A): Flags[A, B] iso^ =>
    """
    This with the flag unset.
    """
    let f = recover Flags[A, B] end
    f._value = this._value and not flag.value()
    f

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
    f._value = this._value xor that._value
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
    Returns true if the flags set on this are a strict subset of the flags set
    on that. Flags is only partially ordered, so lt is not the opposite of ge.
    """
    (_value != that._value) and ((_value and not that._value) == 0)

  fun le(that: Flags[A, B] box): Bool =>
    """
    Returns true if the flags set on this are a subset of the flags set on
    that or they are the same. Flags is only partially ordered, so le is not
    the opposite of te.
    """
    ((_value and not that._value) == 0)

  fun gt(that: Flags[A, B] box): Bool =>
    """
    Returns true if the flags set on this are a struct superset of the flags
    set on that. Flags is only partially ordered, so gt is not the opposite of
    le.
    """
    (_value != that._value) and ((that._value and not _value) == 0)

  fun ge(that: Flags[A, B] box): Bool =>
    """
    Returns true if the flags set on this are a superset of the flags set on
    that or they are the same. Flags is only partially ordered, so ge is not
    the opposite of lt.
    """
    ((that._value and not _value) == 0)
