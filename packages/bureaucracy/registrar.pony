use "collections"
use "promises"

actor Registrar
  let _registry: Map[String, Any tag] = _registry.create()

  be update(key: String, value: Any tag) =>
    _registry(key) = value

  be remove(key: String, value: Any tag) =>
    try
      if _registry(key) is value then
        _registry.remove(key)
      end
    end

  fun tag apply[A: Any tag](key: String): Promise[A] =>
    let promise = Promise[A]
    _fetch[A](key, promise)
    promise

  be _fetch[A: Any tag](key: String, promise: Promise[A]) =>
    try
      promise(_registry(key) as A)
    else
      promise.reject()
    end
