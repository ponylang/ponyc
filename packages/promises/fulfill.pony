primitive _Pending
primitive _Reject

interface iso Fulfill[A: Any #share, B: Any #share]
  """
  A function from A to B that is called when a promise is fulfilled.
  """
  fun ref apply(value: A): B ?

interface iso Reject[A: Any #share]
  """
  A function on A that is called when a promise is rejected.
  """
  fun ref apply(): A ?
