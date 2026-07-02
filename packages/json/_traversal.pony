primitive _Delete

trait val _JsonTraversal
  """
  Internal trait for lens traversal operations.

  apply: read a value at the focus point.
  update: write or delete a value at the focus point, returning a new root.
  """
  fun apply(v: JsonValue): (JsonValue | JsonNotFound)
  fun update(input: JsonValue, value: (JsonValue | _Delete)): (JsonValue | JsonNotFound)
  fun val compose(t: _JsonTraversal): _JsonTraversal => _TravCompose(this, t)
  fun val or_else(alt: _JsonTraversal): _JsonTraversal => _TravChoice(this, alt)

primitive _NoTraversal is _JsonTraversal
  """Identity traversal — returns the input unchanged."""
  fun apply(v: JsonValue): (JsonValue | JsonNotFound) => v
  fun update(input: JsonValue, value: (JsonValue | _Delete)): (JsonValue | JsonNotFound) =>
    match value
    | let j: JsonValue => j
    else JsonNotFound
    end

class val _TravObjKey is _JsonTraversal
  """Focus on an object key."""
  let _key: String

  new val create(key: String) => _key = key

  fun apply(v: JsonValue): (JsonValue | JsonNotFound) =>
    try (v as JsonObject)(_key)?
    else JsonNotFound
    end

  fun update(input: JsonValue, value: (JsonValue | _Delete)): (JsonValue | JsonNotFound) =>
    try
      let obj = input as JsonObject
      match \exhaustive\ value
      | let j: JsonValue => obj.update(_key, j)
      | _Delete => obj.remove(_key)
      end
    else
      JsonNotFound
    end

class val _TravArrayIndex is _JsonTraversal
  """Focus on an array index."""
  let _idx: USize

  new val create(idx: USize) => _idx = idx

  fun apply(v: JsonValue): (JsonValue | JsonNotFound) =>
    try (v as JsonArray)(_idx)?
    else JsonNotFound
    end

  fun update(input: JsonValue, value: (JsonValue | _Delete)): (JsonValue | JsonNotFound) =>
    try
      let arr = input as JsonArray
      match value
      | let j: JsonValue => arr.update(_idx, j)?
      else
        // _Delete (remove) on array index — not supported, return JsonNotFound
        JsonNotFound
      end
    else
      JsonNotFound
    end

class val _TravCompose is _JsonTraversal
  """Sequential composition: navigate _a, then navigate _b within the result."""
  let _a: _JsonTraversal
  let _b: _JsonTraversal

  new val create(a: _JsonTraversal, b: _JsonTraversal) =>
    _a = a
    _b = b

  fun apply(v: JsonValue): (JsonValue | JsonNotFound) =>
    match _a(v)
    | let j: JsonValue => _b(j)
    else JsonNotFound
    end

  fun update(input: JsonValue, value: (JsonValue | _Delete)): (JsonValue | JsonNotFound) =>
    try
      let intermediate = _a(input) as JsonValue
      let inner_result = _b.update(intermediate, value) as JsonValue
      _a.update(input, inner_result)
    else
      JsonNotFound
    end

class val _TravChoice is _JsonTraversal
  """Choice: try _a, fall back to _b if JsonNotFound."""
  let _a: _JsonTraversal
  let _b: _JsonTraversal

  new val create(a: _JsonTraversal, b: _JsonTraversal) =>
    _a = a
    _b = b

  fun apply(v: JsonValue): (JsonValue | JsonNotFound) =>
    match _a(v)
    | let j: JsonValue => j
    else _b(v)
    end

  fun update(input: JsonValue, value: (JsonValue | _Delete)): (JsonValue | JsonNotFound) =>
    match _a(input)
    | let _: JsonValue => _a.update(input, value)
    else _b.update(input, value)
    end
