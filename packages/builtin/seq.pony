interface Seq[A]
  """
  A mutable sequence of elements.

  Types with a fixed number of elements that can be read and written by index
  implement this interface. For sequences that can grow and shrink, see
  ResizableSeq.
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
    Replaces the i-th element of the sequence, returning the previous value.
    Raises an error if the index is out of bounds.
    """

  fun values(): Iterator[this->A]^
    """
    Returns an iterator over the elements of the sequence.
    """
