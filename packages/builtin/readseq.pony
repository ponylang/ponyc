interface box ReadSeq[A]
  """
  The readable interface of a sequence.
  """
  fun size(): USize
    """
    Returns the number of elements in the sequence.
    """

  fun apply(i: USize): this->A ?
    """
    Returns the i-th element of the sequence. Raises an error if the index
    is out of bounds. Note that this returns A^, not this->A.
    """

  fun values(): Iterator[this->A]^
    """
    Returns an iterator over the elements of the sequence. Note that this
    iterates over A^, not this->A.
    """

interface box ReadElement[A]
  """
  Used to show that a ReadSeq can return an element of a specific unmodified
  type.
  """
  fun apply(i: USize): A ?
