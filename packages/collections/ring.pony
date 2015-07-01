class Ring[A]
  """
  A ring buffer.
  """
  let _array: Array[A]
  let _mod: U64
  var _write: U64 = 0

  new create(len: U64) =>
    """
    Create a ring buffer with a fixed size. The size will be rounded up to the
    next power of 2.
    """
    let n = len.max(2).next_pow2()
    _mod = n - 1
    _array = Array[A](n)

  fun head(): U64 ? =>
    """
    The first read that will succeed. If nothing has been written to the ring,
    this will raise an error.
    """
    if _write > 0 then
      if _write > space() then
        _write - space()
      else
        0
      end
    else
      error
    end

  fun size(): U64 =>
    """
    The number of elements that have been added to the ring.
    """
    _write

  fun space(): U64 =>
    """
    The available space in the ring.
    """
    _array.space()

  fun apply(i: U64): this->A ? =>
    """
    Get the i-th element from the ring. If the i-th element has not yet been
    added or is no longer available, this will raise an error.
    """
    if (i >= _write) or ((_write - i) > space()) then
      error
    end

    _array(i and _mod)

  fun ref push(value: A): Ring[A]^ =>
    """
    Add an element to the ring. If the ring is full, this will drop the oldest
    element in the ring.
    """
    if _write < space() then
      _array.push(consume value)
    else
      try _array(_write and _mod) = consume value end
    end

    _write = _write + 1
    this

  fun ref clear(): Ring[A]^ =>
    """
    Clear the queue.
    """
    _array.clear()
    _write = 0
    this
