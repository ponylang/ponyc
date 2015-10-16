primitive _Pending
primitive _Reject

interface Fulfill[A: Any #share, B: Any #share] iso
  """
  A function from A to B that is called when a promise is fulfilled.
  """
  fun ref apply(value: A): B ?

interface Reject[A: Any #share] iso
  """
  A function on A that is called when a promise is rejected.
  """
  fun ref apply(): A ?
