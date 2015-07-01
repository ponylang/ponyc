class Array[A] is Seq[A]
  """
  Contiguous memory to store elements of type A.
  """
  var _size: U64
  var _alloc: U64
  var _ptr: Pointer[A]

  new create(len: U64 = 0) =>
    """
    Create an array with zero elements, but space for len elements.
    """
    _size = 0
    _alloc = len
    _ptr = Pointer[A]._alloc(len)

  new init(from: A^, len: U64) =>
    """
    Create an array of len elements, all initialised to the given value.
    """
    _size = len
    _alloc = len
    _ptr = Pointer[A]._alloc(len)

    var i: U64 = 0

    while i < len do
      _ptr._update(i, from)
      i = i + 1
    end

  new undefined[B: (A & Real[B] box & Number) = A](len: U64) =>
    """
    Create an array of len elements, populating them with random memory. This
    is only allowed for an array of numbers.
    """
    _size = len
    _alloc = len
    _ptr = Pointer[A]._alloc(len)

  new from_cstring(ptr: Pointer[A] ref, len: U64, alloc: U64 = 0) =>
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

  fun cstring(): Pointer[A] tag =>
    """
    Return the underlying C-style pointer.
    """
    _ptr

  fun _cstring(): Pointer[A] box =>
    """
    Internal cstring.
    """
    _ptr

  fun size(): U64 =>
    """
    The number of elements in the array.
    """
    _size

  fun space(): U64 =>
    """
    The available space in the array.
    """
    _alloc

  fun ref reserve(len: U64): Array[A]^ =>
    """
    Reserve space for len elements, including whatever elements are already in
    the array.
    """
    if _alloc < len then
      _alloc = len.max(8).next_pow2()
      _ptr = _ptr._realloc(_alloc)
    end
    this

  fun apply(i: U64): this->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _ptr._apply(i)
    else
      error
    end

  fun ref update(i: U64, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _ptr._update(i, consume value)
    else
      error
    end

  fun ref insert(i: U64, value: A): Array[A]^ ? =>
    """
    Insert an element into the array. Elements after this are moved up by one
    index, extending the array.
    """
    if i < _size then
      reserve(_size + 1)
      _ptr._offset(i)._insert(1, _size - i)
      _ptr._update(i, consume value)
      _size = _size + 1
      this
    else
      error
    end

  fun ref delete(i: U64): A^ ? =>
    """
    Delete an element from the array. Elements after this are moved down by one
    index, compacting the array.
    """
    if i < _size then
      _size = _size - 1
      _ptr._offset(i)._delete(1, _size - i)
    else
      error
    end

  fun ref truncate(len: U64): Array[A]^ =>
    """
    Truncate an array to the given length, discarding excess elements. If the
    array is already smaller than len, do nothing.
    """
    _size = _size.min(len)
    this

  fun copy_to(dst: Array[this->A!], src_idx: U64, dst_idx: U64, len: U64):
    this->Array[A]^
  =>
    """
    Copy len elements from this(src_idx) to dst(dst_idx).
    """
    _ptr._offset(src_idx)._copy_to(dst._ptr._offset(dst_idx), len)
    this

  fun ref remove(i: U64, n: U64): Array[A]^ =>
    """
    Remove n elements from the array, beginning at index i.
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
    """
    _size = 0
    this

  fun ref push(value: A): Array[A]^ =>
    """
    Add an element to the end of the array.
    """
    reserve(_size + 1)
    _ptr._update(_size, consume value)
    _size = _size + 1
    this

  fun ref pop(): A^ ? =>
    """
    Remove an element from the end of the array.
    """
    delete(_size - 1)

  fun ref unshift(value: A): Array[A]^ =>
    """
    Add an element to the beginning of the array.
    """
    try
      insert(0, consume value)
    end
    this

  fun ref shift(): A^ ? =>
    """
    Remove an element from the beginning of the array.
    """
    delete(0)

  fun ref append(seq: ReadSeq[A], offset: U64 = 0, len: U64 = -1): Array[A]^ =>
    """
    Append the elements from a sequence, starting from the given offset.
    """
    if offset >= seq.size() then
      return this
    end

    let copy_len = len.min(seq.size() - offset)
    reserve(_size + copy_len)

    let cap = copy_len + offset
    var i = offset

    try
      while i < cap do
        push(seq(i))
        i = i + 1
      end
    end

    this

  fun ref concat(iter: Iterator[A^]): Array[A]^ =>
    """
    Add iterated elements to the end of the array.
    """
    for v in iter do
      push(consume v)
    end

    this

  fun find(value: A!, offset: U64 = 0, nth: U64 = 0): U64 ? =>
    """
    Find the n-th appearance of value in the array, by identity. Return the
    index, or raise an error if value isn't present.
    """
    var i = offset
    var n = U64(0)

    while i < _size do
      if _ptr._apply(i) is value then
        if n == nth then
          return i
        end

        n = n + 1
      end

      i = i + 1
    end

    error

  fun rfind(value: A!, offset: U64 = -1, nth: U64 = 0): U64 ? =>
    """
    As find, but search backwards in the array.
    """
    if _size > 0 then
      var i = if offset >= _size then _size - 1 else offset end
      var n = U64(0)

      repeat
        if _ptr._apply(i) is value then
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
    """
    let out = Array[this->A!](_size)
    _ptr._copy_to(out._ptr, _size)
    out

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

class ArrayKeys[A, B: Array[A] box] is Iterator[U64]
  let _array: B
  var _i: U64

  new create(array: B) =>
    _array = array
    _i = 0

  fun has_next(): Bool =>
    _i < _array.size()

  fun ref next(): U64 =>
    if _i < _array.size() then
      _i = _i + 1
    else
      _i
    end

class ArrayValues[A, B: Array[A] box] is Iterator[B->A]
  let _array: B
  var _i: U64

  new create(array: B) =>
    _array = array
    _i = 0

  fun has_next(): Bool =>
    _i < _array.size()

  fun ref next(): B->A ? =>
    _array(_i = _i + 1)

class ArrayPairs[A, B: Array[A] box] is Iterator[(U64, B->A)]
  let _array: B
  var _i: U64

  new create(array: B) =>
    _array = array
    _i = 0

  fun has_next(): Bool =>
    _i < _array.size()

  fun ref next(): (U64, B->A) ? =>
    (_i, _array(_i = _i + 1))
