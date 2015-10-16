primitive _Pending
primitive _Reject

interface iso Fulfill[A: Any tag, B: Any tag]
  """
  A function from A to B that is called when a promise is fulfilled.
  """
  fun ref apply(value: A): B ?

interface iso Reject[A: Any tag]
  """
  A function on A that is called when a promise is rejected.
  """
  fun ref apply(): A ?
