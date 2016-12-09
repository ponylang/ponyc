interface Seq[A]
  """
  A sequence of elements.
  """
  new create(len: USize = 0)
    """
    Create a sequence, reserving space for len elements.
    """

  fun ref reserve(len: USize)
    """
    Reserve space for len elements.
    """

  fun size(): USize
    """
    Returns the number of elements in the sequence.
    """

  fun apply(i: USize): this->A ?
    """
    Returns the i-th element of the sequence. Raises an error if the index
    is out of bounds.
    """

  fun ref update(i: USize, value: A): A^ ?
    """
    Replaces the i-th element of the sequence. Returns the previous value.
    Raises an error if the index is out of bounds.
    """

  fun ref clear()
    """
    Removes all elements from the sequence.
    """

  fun ref push(value: A)
    """
    Adds an element to the end of the sequence.
    """

  fun ref pop(): A^ ?
    """
    Removes an element from the end of the sequence.
    """

  fun ref unshift(value: A)
    """
    Adds an element to the beginning of the sequence.
    """

  fun ref shift(): A^ ?
    """
    Removes an element from the beginning of the sequence.
    """

  fun ref append(seq: (ReadSeq[A] & ReadElement[A^]), offset: USize = 0,
    len: USize = -1)
    """
    Add len elements to the end of the list, starting from the given
    offset.
    """

  fun ref concat(iter: Iterator[A^], offset: USize = 0, len: USize = -1)
    """
    Add len iterated elements to the end of the list, starting from the given
    offset.
    """

  fun ref truncate(len: USize)
    """
    Truncate the sequence to the given length, discarding excess elements.
    If the sequence is already smaller than len, do nothing.
    """

  fun values(): Iterator[this->A]^
    """
    Returns an iterator over the elements of the sequence.
    """
