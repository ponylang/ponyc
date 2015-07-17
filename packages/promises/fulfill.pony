primitive _Pending
primitive _Reject

interface Fulfill[A: Any tag, B: Any tag] iso
  """
  A function from A to B that is called when a promise is fulfilled.
  """
  fun ref apply(value: A): B ?

interface Reject[A: Any tag] iso
  """
  A function on A that is called when a promise is rejected.
  """
  fun ref apply(): A ?
