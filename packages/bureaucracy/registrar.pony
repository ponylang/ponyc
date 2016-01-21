use "collections"
use "promises"

actor Registrar
  """
  A Registrar keeps a map of lookup string to anything. Generally, this is used
  to keep a directory of long-lived service-providing actors that can be
  looked up name.
  """
  let _registry: Map[String, Any tag] = _registry.create()

  be update(key: String, value: Any tag) =>
    """
    Add, or change, a lookup mapping.
    """
    _registry(key) = value

  be remove(key: String, value: Any tag) =>
    """
    Remove a mapping. This only takes effect if provided key currently maps to
    the provided value. If the key maps to some other value (perhaps after
    updating), the mapping won't be removed.
    """
    try
      if _registry(key) is value then
        _registry.remove(key)
      end
    end

  fun tag apply[A: Any tag = Any tag](key: String): Promise[A] =>
    """
    Lookup by name. Returns a promise that will be fulfilled with the mapped
    value if it exists and is a subtype of A. Otherwise, the promise will be
    rejected.
    """
    let promise = Promise[A]
    _fetch[A](key, promise)
    promise

  be _fetch[A: Any tag](key: String, promise: Promise[A]) =>
    """
    Fulfills or rejects the promise.
    """
    try
      promise(_registry(key) as A)
    else
      promise.reject()
    end
