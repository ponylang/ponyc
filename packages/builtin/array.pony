class Array[A] is Seq[A]
  """
  Contiguous, resizable memory to store elements of type A.
  """
  var _size: USize
  var _alloc: USize
  var _ptr: Pointer[A]

  new create(len: USize = 0) =>
    """
    Create an array with zero elements, but space for len elements.
    """
    _size = 0

    if len > 0 then
      _alloc = len.next_pow2().max(len).max(8)
      _ptr = Pointer[A]._alloc(_alloc)
    else
      _alloc = 0
      _ptr = Pointer[A]
    end

  new init(from: A^, len: USize) =>
    """
    Create an array of len elements, all initialised to the given value.
    """
    _size = len

    if len > 0 then
      _alloc = len.next_pow2().max(len).max(8)
      _ptr = Pointer[A]._alloc(_alloc)

      var i: USize = 0

      while i < len do
        _ptr._update(i, from)
        i = i + 1
      end
    else
      _alloc = 0
      _ptr = Pointer[A]
    end

  new from_cpointer(ptr: Pointer[A], len: USize, alloc: USize = 0) =>
    """
    Create an array from a C-style pointer and length. The contents are not
    copied.
    """
    _size = len

    if alloc > len then
      _alloc = alloc
    else
      _alloc = len
    end

    _ptr = ptr

  fun _copy_to(ptr: Pointer[this->A!], copy_len: USize,
    from_offset: USize = 0, to_offset: USize = 0) =>
    """
    Copy copy_len elements from this to that at specified offsets.
    """
    _ptr._offset(from_offset)._copy_to(ptr._offset(to_offset), copy_len)

  fun cpointer(): Pointer[A] tag =>
    """
    Return the underlying C-style pointer.
    """
    _ptr

  fun size(): USize =>
    """
    The number of elements in the array.
    """
    _size

  fun space(): USize =>
    """
    The available space in the array.
    """
    _alloc

  fun ref reserve(len: USize): Array[A]^ =>
    """
    Reserve space for len elements, including whatever elements are already in
    the array. Array space grows geometrically.
    """
    if _alloc < len then
      _alloc = len.next_pow2().max(len).max(8)
      _ptr = _ptr._realloc(_alloc)
    end
    this

  fun ref compact(): Array[A]^ =>
    """
    Try to remove unused space, making it available for garbage collection. The
    request may be ignored. The array is returned to allow call chaining.
    """
    if _size <= (512 / _ptr._element_size()) then
      if _size.next_pow2() != _alloc.next_pow2() then
        _alloc = _size.next_pow2()
        let old_ptr = _ptr = Pointer[A]._alloc(_alloc)
        _ptr._consume_from(consume old_ptr, _size)
      end
    elseif _size < _alloc then
      _alloc = _size
      let old_ptr = _ptr = Pointer[A]._alloc(_alloc)
      _ptr._consume_from(consume old_ptr, _size)
    end
    this

  fun ref undefined[B: (A & Real[B] val & Number) = A](len: USize): Array[A]^
  =>
    """
    Resize to len elements, populating previously empty elements with random
    memory. This is only allowed for an array of numbers.
    """
    reserve(len)
    _size = len
    this

  fun apply(i: USize): this->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _ptr._apply(i)
    else
      error
    end

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _ptr._update(i, consume value)
    else
      error
    end

  fun ref insert(i: USize, value: A): Array[A]^ ? =>
    """
    Insert an element into the array. Elements after this are moved up by one
    index, extending the array.
    An out of bounds index raises an error.
    The array is returned to allow call chaining.
    """
    if i <= _size then
      reserve(_size + 1)
      _ptr._offset(i)._insert(1, _size - i)
      _ptr._update(i, consume value)
      _size = _size + 1
      this
    else
      error
    end

  fun ref delete(i: USize): A^ ? =>
    """
    Delete an element from the array. Elements after this are moved down by one
    index, compacting the array.
    An out of bounds index raises an error.
    The deleted element is returned.
    """
    if i < _size then
      _size = _size - 1
      _ptr._offset(i)._delete(1, _size - i)
    else
      error
    end

  fun ref truncate(len: USize): Array[A]^ =>
    """
    Truncate an array to the given length, discarding excess elements. If the
    array is already smaller than len, do nothing.
    The array is returned to allow call chaining.
    """
    _size = _size.min(len)
    this

  fun ref trim_in_place(from: USize = 0, to: USize = -1): Array[A]^ =>
    """
    Trim the array to a portion of itself, covering `from` until `to`.
    Unlike slice, the operation does not allocate a new array nor copy elements.
    The same array is returned to allow call chaining.
    """
    let last = _size.min(to)
    let offset = last.min(from)

    _size = last - offset
    _alloc = _alloc - offset
    _ptr = if _size > 0 then _ptr._offset(offset) else _ptr.create() end

    this

  fun val trim(from: USize = 0, to: USize = -1): Array[A] val =>
    """
    Return a shared portion of this array, covering `from` until `to`.
    Both the original and the new array are immutable, as they share memory.
    The operation does not allocate a new array pointer nor copy elements.
    """
    let last = _size.min(to)
    let offset = last.min(from)

    recover
      let size' = last - offset
      let alloc = _alloc - offset

      if size' > 0 then
        from_cpointer(_ptr._offset(offset)._unsafe(), size', alloc)
      else
        create()
      end
    end

  fun copy_to(dst: Array[this->A!], src_idx: USize, dst_idx: USize,
    len: USize): this->Array[A]^
  =>
    """
    Copy len elements from this(src_idx) to dst(dst_idx).
    The array is returned to allow call chaining.
    """
    dst.reserve(dst_idx + len)
    _ptr._offset(src_idx)._copy_to(dst._ptr._offset(dst_idx), len)

    if dst._size < (dst_idx + len) then
      dst._size = dst_idx + len
    end
    this

  fun ref remove(i: USize, n: USize): Array[A]^ =>
    """
    Remove n elements from the array, beginning at index i.
    The array is returned to allow call chaining.
    """
    if i < _size then
      let count = n.min(_size - i)
      _size = _size - count
      _ptr._offset(i)._delete(count, _size - i)
    end
    this

  fun ref clear(): Array[A]^ =>
    """
    Remove all elements from the array.
    The array is returned to allow call chaining.
    """
    _size = 0
    this

  fun ref push(value: A): Array[A]^ =>
    """
    Add an element to the end of the array.
    The array is returned to allow call chaining.
    """
    reserve(_size + 1)
    _ptr._update(_size, consume value)
    _size = _size + 1
    this

  fun ref pop(): A^ ? =>
    """
    Remove an element from the end of the array.
    The removed element is returned.
    """
    delete(_size - 1)

  fun ref unshift(value: A): Array[A]^ =>
    """
    Add an element to the beginning of the array.
    The array is returned to allow call chaining.
    """
    try
      insert(0, consume value)
    end
    this

  fun ref shift(): A^ ? =>
    """
    Remove an element from the beginning of the array.
    The removed element is returned.
    """
    delete(0)

  fun ref append(seq: (ReadSeq[A] & ReadElement[A^]), offset: USize = 0,
    len: USize = -1): Array[A]^
  =>
    """
    Append the elements from a sequence, starting from the given offset.
    The array is returned to allow call chaining.
    """
    if offset >= seq.size() then
      return this
    end

    let copy_len = len.min(seq.size() - offset)
    reserve(_size + copy_len)

    var n = USize(0)

    try
      while n < copy_len do
        _ptr._update(_size + n, seq(offset + n))

        n = n + 1
      end
    end

    _size = _size + n

    this

  fun ref concat(iter: Iterator[A^], offset: USize = 0, len: USize = -1)
    : Array[A]^
  =>
    """
    Add len iterated elements to the end of the array, starting from the given
    offset. The array is returned to allow call chaining.
    """

    var n = USize(0)

    try
      while n < offset do
        if iter.has_next() then
          iter.next()
        else
          return this
        end

        n = n + 1
      end
    end

    n = 0

    // If a concrete len is specified, we take the caller at their word
    // and reserve that much space, even though we can't verify that the
    // iterator actually has that many elements available. Reserving ahead
    // of time lets us take a fast path of direct pointer access.
    if len != -1 then
      reserve(_size + len)

      try
        while n < len do
          if iter.has_next() then
            _ptr._update(_size + n, iter.next())
          else
            break
          end

          n = n + 1
        end
      end

      _size = _size + n
    else
      try
        while n < len do
          if iter.has_next() then
            push(iter.next())
          else
            break
          end

          n = n + 1
        end
      end
    end

    this

  fun find(value: A!, offset: USize = 0, nth: USize = 0,
    predicate: {(box->A!, box->A!): Bool} val =
      {(l: box->A!, r: box->A!): Bool => l is r }): USize ?
  =>
    """
    Find the `nth` appearance of `value` from the beginning of the array,
    starting at `offset` and examining higher indices, and using the supplied
    `predicate` for comparisons. Returns the index of the value, or raise an
    error if the value isn't present.

    By default, the search starts at the first element of the array, returns the
    first instance of `value` found, and uses object identity for comparison.
    """
    var i = offset
    var n = USize(0)

    while i < _size do
      if predicate(_ptr._apply(i), value) then
        if n == nth then
          return i
        end

        n = n + 1
      end

      i = i + 1
    end

    error

  fun contains(value: A!, predicate: {(box->A!, box->A!): Bool} val =
    {(l: box->A!, r: box->A!): Bool => l is r }): Bool =>
    """
    Returns true if the array contains `value`, false otherwise.
    """
    var i = USize(0)

    while i < _size do
      if predicate(_ptr._apply(i), value) then
        return true
      end

      i = i + 1
    end
    false

  fun rfind(value: A!, offset: USize = -1, nth: USize = 0,
    predicate: {(box->A!, box->A!): Bool} val =
      {(l: box->A!, r: box->A!): Bool => l is r }): USize ?
  =>
    """
    Find the `nth` appearance of `value` from the end of the array, starting at
    `offset` and examining lower indices, and using the supplied `predicate` for
    comparisons. Returns the index of the value, or raise an error if the value
    isn't present.

    By default, the search starts at the last element of the array, returns the
    first instance of `value` found, and uses object identity for comparison.
    """
    if _size > 0 then
      var i = if offset >= _size then _size - 1 else offset end
      var n = USize(0)

      repeat
        if predicate(_ptr._apply(i), value) then
          if n == nth then
            return i
          end

          n = n + 1
        end
      until (i = i - 1) == 0 end
    end

    error

  fun clone(): Array[this->A!]^ =>
    """
    Clone the array.
    The new array contains references to the same elements that the old array
    contains, the elements themselves are not cloned.
    """
    let out = Array[this->A!](_size)
    _ptr._copy_to(out._ptr, _size)
    out._size = _size
    out

  fun slice(from: USize = 0, to: USize = -1, step: USize = 1)
    : Array[this->A!]^
  =>
    """
    Create a new array that is a clone of a portion of this array. The range is
    exclusive and saturated.
    The new array contains references to the same elements that the old array
    contains, the elements themselves are not cloned.
    """
    let out = Array[this->A!]
    let last = _size.min(to)
    let len = last - from

    if (last > from) and (step > 0) then
      out.reserve((len + (step - 1)) / step)

      if step == 1 then
        copy_to(out, from, 0, len)
      else
        try
          var i = from

          while i < last do
            out.push(this(i))
            i = i + step
          end
        end
      end
    end

    out

  fun permute(indices: Iterator[USize]): Array[this->A!]^ ? =>
    """
    Create a new array with the elements permuted.
    Permute to an arbitrary order that may include duplicates. An out of bounds
    index raises an error.
    The new array contains references to the same elements that the old array
    contains, the elements themselves are not copied.
    """
    let out = Array[this->A!]
    for i in indices do
      out.push(this(i))
    end
    out

  fun reverse(): Array[this->A!]^ =>
    """
    Create a new array with the elements in reverse order.
    The new array contains references to the same elements that the old array
    contains, the elements themselves are not copied.
    """
    clone().reverse_in_place()

  fun ref reverse_in_place(): Array[A] ref^ =>
    """
    Reverse the array in place.
    """
    if _size > 1 then
      var i: USize = 0
      var j = _size - 1

      while i < j do
        let x = _ptr._apply(i)
        _ptr._update(i, _ptr._apply(j))
        _ptr._update(j, x)
        i = i + 1
        j = j - 1
      end
    end
    this

  fun keys(): ArrayKeys[A, this->Array[A]]^ =>
    """
    Return an iterator over the indices in the array.
    """
    ArrayKeys[A, this->Array[A]](this)

  fun values(): ArrayValues[A, this->Array[A]]^ =>
    """
    Return an iterator over the values in the array.
    """
    ArrayValues[A, this->Array[A]](this)

  fun pairs(): ArrayPairs[A, this->Array[A]]^ =>
    """
    Return an iterator over the (index, value) pairs in the array.
    """
    ArrayPairs[A, this->Array[A]](this)

class ArrayKeys[A, B: Array[A] #read] is Iterator[USize]
  let _array: B
  var _i: USize

  new create(array: B) =>
    _array = array
    _i = 0

  fun has_next(): Bool =>
    _i < _array.size()

  fun ref next(): USize =>
    if _i < _array.size() then
      _i = _i + 1
    else
      _i
    end

class ArrayValues[A, B: Array[A] #read] is Iterator[B->A]
  let _array: B
  var _i: USize

  new create(array: B) =>
    _array = array
    _i = 0

  fun has_next(): Bool =>
    _i < _array.size()

  fun ref next(): B->A ? =>
    _array(_i = _i + 1)

  fun ref rewind(): ArrayValues[A, B] =>
    _i = 0
    this

class ArrayPairs[A, B: Array[A] #read] is Iterator[(USize, B->A)]
  let _array: B
  var _i: USize

  new create(array: B) =>
    _array = array
    _i = 0

  fun has_next(): Bool =>
    _i < _array.size()

  fun ref next(): (USize, B->A) ? =>
    (_i, _array(_i = _i + 1))
