class Array[A] is Seq[A]
  """
  Contiguous, resizable memory to store elements of type A.

  ## Usage

  Creating an Array of String.
  ```pony
    let array: Array[String] = ["dog"; "cat"; "wombat"]
    // array.size() == 3
    // array.space() >= 3
  ```

  Creating an empty Array of String, which may hold at least 10 elements before
  requesting more space.
  ```pony
    let array = Array(10)
    // array.size() == 0
    // array.space() >= 10
  ```

  Accessing elements can be done via the `apply(i: USize): this->A ?` method.
  The provided index might be out of bounds so `apply` is partial and has to be
  called within a try-catch block or inside another partial method.
  ```pony
    let array: Array[String] = ["dog"; "cat"; "wombat"]
    let is_second_element_wobat = try
      // indexes start from 0, so 1 is the second element
      array(1)? == "wombat"
    else
      false
    end
  ```

  Adding and removing elements to and from the end of the Array can be done via
  `push` and `pop` methods. You could treat the array as a FIFO queue using
  those methods.
  ```pony
    while (array.size() > 0) do
      let elem = array.pop()?
      // do something with element
    end
  ```

  Modifying the Array can be done via `update`, `insert` and `delete` methods
  which alter the Array at an arbitrary index, moving elements left (when
  deleting) or right (when inserting) as necessary.

  Iterating over the elements of an Array can be done using the `values` method.
  ```pony
    for element in array.values() do
        // do something with element
    end
  ```

  ## Memory allocation
  Array allocates contiguous memory. It always allocates at least enough memory
  space to hold all of its elements. Space is the number of elements the Array
  can hold without allocating more memory. The `space()` method returns the
  number of elements an Array can hold. The `size()` method returns the number
  of elements the Array holds.

  Different data types require different amounts of memory. Array[U64] with size
  of 6 will take more memory than an Array[U8] of the same size.

  When creating an Array or adding more elements will calculate the next power
  of 2 of the requested number of elements and allocate that much space, with a
  lower bound of space for 8 elements.

  Here's a few examples of the space allocated when initialising an Array with
  various number of elements:

  | size | space |
  |------|-------|
  | 0    | 0     |
  | 1    | 8     |
  | 8    | 8     |
  | 9    | 16    |
  | 16   | 16    |
  | 17   | 32    |

  Call the `compact()` method to ask the GC to reclaim unused space. There are
  no guarantees that the GC will actually reclaim any space.
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

  fun _copy_to(
    ptr: Pointer[this->A!],
    copy_len: USize,
    from_offset: USize = 0,
    to_offset: USize = 0)
  =>
    """
    Copy copy_len elements from this to that at specified offsets.
    """
    _ptr._offset(from_offset)._copy_to(ptr._offset(to_offset), copy_len)

  fun cpointer(offset: USize = 0): Pointer[A] tag =>
    """
    Return the underlying C-style pointer.
    """
    _ptr._offset(offset)

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

  fun ref reserve(len: USize) =>
    """
    Reserve space for len elements, including whatever elements are already in
    the array. Array space grows geometrically.
    """
    if _alloc < len then
      _alloc = len.next_pow2().max(len).max(8)
      _ptr = _ptr._realloc(_alloc)
    end

  fun ref compact() =>
    """
    Try to remove unused space, making it available for garbage collection. The
    request may be ignored.
    """
    if _size <= (512 / _ptr._element_size()) then
      if _size.next_pow2() != _alloc.next_pow2() then
        _alloc = _size.next_pow2()
        let old_ptr = _ptr = Pointer[A]._alloc(_alloc)
        old_ptr._copy_to(_ptr._convert[A!](), _size)
      end
    elseif _size < _alloc then
      _alloc = _size
      let old_ptr = _ptr = Pointer[A]._alloc(_alloc)
      old_ptr._copy_to(_ptr._convert[A!](), _size)
    end

  fun ref undefined[B: (A & Real[B] val & Number) = A](len: USize) =>
    """
    Resize to len elements, populating previously empty elements with random
    memory. This is only allowed for an array of numbers.
    """
    reserve(len)
    _size = len

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

  fun ref insert(i: USize, value: A) ? =>
    """
    Insert an element into the array. Elements after this are moved up by one
    index, extending the array.
    An out of bounds index raises an error.
    """
    if i <= _size then
      reserve(_size + 1)
      _ptr._offset(i)._insert(1, _size - i)
      _ptr._update(i, consume value)
      _size = _size + 1
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

  fun ref truncate(len: USize) =>
    """
    Truncate an array to the given length, discarding excess elements. If the
    array is already smaller than len, do nothing.
    """
    _size = _size.min(len)

  fun ref trim_in_place(from: USize = 0, to: USize = -1) =>
    """
    Trim the array to a portion of itself, covering `from` until `to`.
    Unlike slice, the operation does not allocate a new array nor copy elements.
    """
    let last = _size.min(to)
    let offset = last.min(from)
    let size' = last - offset

    // use the new size' for alloc if we're not including the last used byte
    // from the original data and only include the extra allocated bytes if
    // we're including the last byte.
    _alloc = if last == _size then _alloc - offset else size' end

    _size = size'
    _ptr = _ptr._offset(offset)

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

      // use the new size' for alloc if we're not including the last used byte
      // from the original data and only include the extra allocated bytes if
      // we're including the last byte.
      let alloc = if last == _size then _alloc - offset else size' end

      if size' > 0 then
        from_cpointer(_ptr._offset(offset)._unsafe(), size', alloc)
      else
        create()
      end
    end

  fun iso chop(split_point: USize): (Array[A] iso^, Array[A] iso^) =>
    """
    Chops the array in half at the split point requested and returns both
    the left and right portions. The original array is trimmed in place and
    returned as the right portion. If the split point is larger than the
    array, the left portion is the original array and the right portion
    is a new empty array.
    Both arrays are isolated and mutable, as they do not share memory.
    The operation does not allocate a new array pointer nor copy elements.
    """
    let start_ptr = _ptr
    let size' = _size.min(split_point)
    let alloc = if size' == _size then _alloc else split_point end

    trim_in_place(split_point)

    let left = recover
      if size' > 0 then
        from_cpointer(start_ptr._unsafe(), size', alloc)
      else
        create()
      end
    end

    (consume left, consume this)

  fun copy_to(
    dst: Array[this->A!],
    src_idx: USize,
    dst_idx: USize,
    len: USize)
  =>
    """
    Copy len elements from this(src_idx) to dst(dst_idx).
    """
    dst.reserve(dst_idx + len)
    _ptr._offset(src_idx)._copy_to(dst._ptr._offset(dst_idx), len)

    if dst._size < (dst_idx + len) then
      dst._size = dst_idx + len
    end

  fun ref remove(i: USize, n: USize) =>
    """
    Remove n elements from the array, beginning at index i.
    """
    if i < _size then
      let count = n.min(_size - i)
      _size = _size - count
      _ptr._offset(i)._delete(count, _size - i)
    end

  fun ref clear() =>
    """
    Remove all elements from the array.
    """
    _size = 0

  fun ref push(value: A) =>
    """
    Add an element to the end of the array.
    """
    reserve(_size + 1)
    _ptr._update(_size, consume value)
    _size = _size + 1

  fun ref pop(): A^ ? =>
    """
    Remove an element from the end of the array.
    The removed element is returned.
    """
    delete(_size - 1)?

  fun ref unshift(value: A) =>
    """
    Add an element to the beginning of the array.
    """
    try
      insert(0, consume value)?
    end

  fun ref shift(): A^ ? =>
    """
    Remove an element from the beginning of the array.
    The removed element is returned.
    """
    delete(0)?

  fun ref append(
    seq: (ReadSeq[A] & ReadElement[A^]),
    offset: USize = 0,
    len: USize = -1)
  =>
    """
    Append the elements from a sequence, starting from the given offset.
    """
    if offset >= seq.size() then
      return
    end

    let copy_len = len.min(seq.size() - offset)
    reserve(_size + copy_len)

    var n = USize(0)

    try
      while n < copy_len do
        _ptr._update(_size + n, seq(offset + n)?)

        n = n + 1
      end
    end

    _size = _size + n

  fun ref concat(iter: Iterator[A^], offset: USize = 0, len: USize = -1) =>
    """
    Add len iterated elements to the end of the array, starting from the given
    offset.
    """

    var n = USize(0)

    try
      while n < offset do
        if iter.has_next() then
          iter.next()?
        else
          return
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
            _ptr._update(_size + n, iter.next()?)
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
            push(iter.next()?)
          else
            break
          end

          n = n + 1
        end
      end
    end

  fun find(
    value: A!,
    offset: USize = 0,
    nth: USize = 0,
    predicate: {(box->A!, box->A!): Bool} val = {(l, r) => l is r })
    : USize ?
  =>
    """
    Find the `nth` appearance of `value` from the beginning of the array,
    starting at `offset` and examining higher indices, and using the supplied
    `predicate` for comparisons. Returns the index of the value, or raise an
    error if the value isn't present.

    By default, the search starts at the first element of the array, returns
    the first instance of `value` found, and uses object identity for
    comparison.
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

  fun contains(
    value: A!,
    predicate: {(box->A!, box->A!): Bool} val =
      {(l: box->A!, r: box->A!): Bool => l is r })
    : Bool
  =>
    """
    Returns true if the array contains `value`, false otherwise.

    The default predicate checks for matches by identity. To search for matches
    by structural equality, pass an object literal such as `{(l, r) => l == r}`.
    """
    var i = USize(0)

    while i < _size do
      if predicate(_ptr._apply(i), value) then
        return true
      end

      i = i + 1
    end
    false

  fun rfind(
    value: A!,
    offset: USize = -1,
    nth: USize = 0,
    predicate: {(box->A!, box->A!): Bool} val =
      {(l: box->A!, r: box->A!): Bool => l is r })
    : USize ?
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
      until (i = i - 1) == 0
      end
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

  fun slice(
    from: USize = 0,
    to: USize = -1,
    step: USize = 1)
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
            out.push(this(i)?)
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
      out.push(this(i)?)
    end
    out

  fun reverse(): Array[this->A!]^ =>
    """
    Create a new array with the elements in reverse order.
    The new array contains references to the same elements that the old array
    contains, the elements themselves are not copied.
    """
    clone() .> reverse_in_place()

  fun ref reverse_in_place() =>
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

  fun ref swap_elements(i: USize, j: USize) ? =>
    """
    Swap the element at index i with the element at index j.
    If either i or j are out of bounds, an error is raised.
    """
    if (i >= _size) or (j >= _size) then error end

    let x = _ptr._apply(i)
    _ptr._update(i, _ptr._apply(j))
    _ptr._update(j, consume x)

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
    _array(_i = _i + 1)?

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
    (_i, _array(_i = _i + 1)?)
