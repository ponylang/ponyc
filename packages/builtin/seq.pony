interface Seq[A]
  """
  A sequence of elements.
  """
  new create(len: U64 = 0)
    """
    Create a sequence, reserving space for len elements.
    """

  fun ref reserve(len: U64): Seq[A]^
    """
    Reserve space for len elements.
    """

  fun size(): U64
    """
    Returns the number of elements in the sequence.
    """

  fun apply(i: U64): this->A ?
    """
    Returns the i-th element of the sequence. Raises an error if the index
    is out of bounds.
    """

  fun ref update(i: U64, value: A): (A^ | None) ?
    """
    Replaces the i-th element of the sequence. Returns the previous value, or
    None if the sequence element exists but has been invalidated. Raises an
    error if the index is out of bounds.
    """

  fun ref clear(): Seq[A]^
    """
    Removes all elements from the sequence.
    """

  fun ref push(value: A): Seq[A]^
    """
    Adds an element to the end of the sequence.
    """

  fun ref pop(): A^ ?
    """
    Removes an element from the end of the sequence.
    """

  fun ref unshift(value: A): Seq[A]^
    """
    Adds an element to the beginning of the sequence.
    """

  fun ref shift(): A^ ?
    """
    Removes an element from the beginning of the sequence.
    """

  fun ref append(seq: ReadSeq[A], offset: U64 = 0, len: U64 = -1): Seq[A]^
    """
    Append the elements from a sequence, starting from the given offset.
    """

  fun values(): Iterator[this->A]^
    """
    Returns an iterator over the elements of the sequence.
    """
