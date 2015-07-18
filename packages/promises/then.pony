class _Then[A: Any tag, B: Any tag]
  """
  A step in a promise pipeline.
  """
  let _fulfill: (Fulfill[A, B] | None)
  let _rejected: (Reject[B] | None)
  let _promise: Promise[B]
  var _active: Bool = true

  new iso create(fulfill: (Fulfill[A, B] | None) = None,
    rejected: (Reject[B] | None) = None)
  =>
    """
    A step is represented by a fulfill function and a reject function.
    """
    _fulfill = consume fulfill
    _rejected = consume rejected
    _promise = Promise[B]

  fun promise(): Promise[B] =>
    """
    Returns the next promise in the chain.
    """
    _promise

  fun ref apply(value: A) =>
    """
    Called with the result of the previous promise when it is fulfilled.
    """
    if _active then
      _active = false

      try
        _promise((_fulfill as Fulfill[A, B])(value))
      else
        _promise.reject()
      end
    end

  fun ref reject() =>
    """
    Called when the previous promise is rejected.
    """
    if _active then
      _active = false

      try
        _promise((_rejected as Reject[B])())
      else
        _promise.reject()
      end
    end

interface _IThen[A: Any tag]
  """
  An interface representing an abstract Then. This allows for any Then that
  accepts an input of type A, regardless of the output type.
  """
  fun ref apply(value: A)
  fun ref reject()
