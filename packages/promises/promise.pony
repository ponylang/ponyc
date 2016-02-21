use "collections"

actor Promise[A: Any #share]
  """
  A promise to eventually produce a result of type A. This promise can either
  be fulfilled or rejected.

  Any number of promises can be
  """
  var _value: (_Pending | _Reject | A) = _Pending
  let _list: List[_IThen[A]] = _list.create()

  be apply(value: A) =>
    """
    Fulfill the promise.
    """
    if _value isnt _Pending then
      return
    end

    _value = value

    for f in _list.values() do
      f(value)
    end

    _list.clear()

  be reject() =>
    """
    Reject the promise.
    """
    if _value isnt _Pending then
      return
    end

    _value = _Reject

    for f in _list.values() do
      f.reject()
    end

    _list.clear()

  fun tag next[B: Any #share](fulfill: Fulfill[A, B],
    rejected: Reject[B] = RejectAlways[B]): Promise[B]
  =>
    """
    Chain a promise after this one.

    When this promise is fulfilled, the result of type A is passed to the
    fulfill function, generating in an intermediate result of type B. This
    is then used to fulfill the next promise in the chain.

    If there is no fulfill function, or if the fulfill function raises an
    error, then the next promise in the chain will be rejected.

    If this promise is rejected, this step's reject function is called with no
    input, generating an intermediate result of type B which is used to
    fulfill the next promise in the chain.

    If there is no reject function, of if the reject function raises an error,
    then the next promise in the chain will be rejected.
    """
    let attach = _Then[A, B](consume fulfill, consume rejected)
    let promise = attach.promise()
    _attach(consume attach)
    promise

  be _attach(attach: _IThen[A] iso) =>
    """
    Attaches a step asynchronously. If this promise has already been fulfilled
    or rejected, immediately fulfill or reject the incoming step. Otherwise,
    keep it in a list.
    """
    if _value is _Pending then
      _list.push(consume attach)
    elseif _value is _Reject then
      attach.reject()
    else
      try attach(_value as A) end
    end
