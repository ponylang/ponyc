primitive ReadBufferResized
  """
  A successful read buffer operation.
  """

primitive ReadBufferResizeBelowBufferSize
  """
  The requested read buffer size or minimum is smaller than the current
  buffer-until value. The buffer-until value sets a hard floor — the buffer must
  be able to hold at least that many bytes to satisfy the framing contract.
  """

primitive ReadBufferResizeBelowUsed
  """
  The requested read buffer size is smaller than the amount of unprocessed
  data currently in the buffer. Honoring the request would truncate data.
  """

type ReadBufferResizeResult is
  ( ReadBufferResized
  | ReadBufferResizeBelowBufferSize
  | ReadBufferResizeBelowUsed )

primitive BufferUntilSet
  """
  A successful buffer_until operation.
  """

primitive BufferSizeAboveMinimum
  """
  The requested `BufferSize` value exceeds the current read buffer minimum.
  Raise the buffer minimum first, then set buffer_until.
  """

type BufferUntilResult is
  (BufferUntilSet | BufferSizeAboveMinimum)
