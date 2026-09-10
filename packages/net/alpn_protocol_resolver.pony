interface val ALPNProtocolResolver
  """
  Controls the protocol name to be chosen for incoming SSL connections using the
  ALPN extension.

  An implementation is `val`: a context shares the resolver across actors and
  runs it from any of them, so it cannot depend on mutable state.
  """
  fun box resolve(advertised: Array[ALPNProtocolName] val): ALPNMatchResult
    """
    Choose a protocol from the ones the client advertised. Returning a name that
    the client did not advertise fails the handshake.
    """

class val ALPNStandardProtocolResolver is ALPNProtocolResolver
  """
  Selects the first supported protocol the client advertised, in the order
  `supported` gives them. Falls back to the client's first choice when none of
  them is supported, unless `use_client_as_fallback` is false, in which case it
  returns `ALPNWarning`.
  """
  let supported: Array[ALPNProtocolName] val
  let use_client_as_fallback: Bool

  new val create(
    supported': Array[ALPNProtocolName] val,
    use_client_as_fallback': Bool = true)
  =>
    supported = supported'
    use_client_as_fallback = use_client_as_fallback'

  fun box resolve(advertised: Array[ALPNProtocolName] val): ALPNMatchResult =>
    for sup_proto in supported.values() do
      for adv_proto in advertised.values() do
        if sup_proto == adv_proto then return sup_proto end
      end
    end
    if use_client_as_fallback then
      try return advertised(0)? end
    end

    ALPNWarning
