primitive _Pending
primitive _Reject

interface iso Fulfill[A: Any #send, B: Any #send]
  """
  A function from A to B that is called when a promise is fulfilled.
  """
  fun ref apply(value: A): B^ ?

interface iso Reject[A: Any #send]
  """
  A function on A that is called when a promise is rejected.
  """
  fun ref apply(): A^ ?

class iso FulfillIdentity[A: Any #send]
  """
  An identity function for fulfilling promises.
  """
  fun ref apply(value: A): A^ =>
    consume value

class iso RejectAlways[A: Any #send]
  """
  A reject that always raises an error.
  """
  fun ref apply(): A^ ? =>
    error
