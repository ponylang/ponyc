primitive _Delete

trait val _JSONTraversal
  """
  Internal trait for lens traversal operations.

  apply: read a value at the focus point.
  update: write or delete a value at the focus point, returning a new root.
  """
  fun apply(v: JSONValue): (JSONValue | JSONNotFound)

  fun update(
    input: JSONValue,
    value: (JSONValue | _Delete))
    : (JSONValue | JSONNotFound)

  fun val compose(t: _JSONTraversal): _JSONTraversal =>
    _TravCompose(this, t)

  fun val or_else(alt: _JSONTraversal): _JSONTraversal =>
    _TravChoice(this, alt)

primitive _NoTraversal is _JSONTraversal
  """
  Identity traversal — returns the input unchanged.
  """
  fun apply(v: JSONValue): (JSONValue | JSONNotFound) => v

  fun update(
    input: JSONValue,
    value: (JSONValue | _Delete))
    : (JSONValue | JSONNotFound)
  =>
    match value
    | let j: JSONValue => j
    else JSONNotFound
    end

class val _TravObjKey is _JSONTraversal
  """
  Focus on an object key.
  """
  let _key: String

  new val create(key: String) => _key = key

  fun apply(v: JSONValue): (JSONValue | JSONNotFound) =>
    try (v as JSONObject)(_key)?
    else JSONNotFound
    end

  fun update(
    input: JSONValue,
    value: (JSONValue | _Delete))
    : (JSONValue | JSONNotFound)
  =>
    try
      let obj = input as JSONObject
      match \exhaustive\ value
      | let j: JSONValue => obj.update(_key, j)
      | _Delete => obj.remove(_key)
      end
    else
      JSONNotFound
    end

class val _TravArrayIndex is _JSONTraversal
  """
  Focus on an array index.
  """
  let _idx: USize

  new val create(idx: USize) => _idx = idx

  fun apply(v: JSONValue): (JSONValue | JSONNotFound) =>
    try (v as JSONArray)(_idx)?
    else JSONNotFound
    end

  fun update(
    input: JSONValue,
    value: (JSONValue | _Delete))
    : (JSONValue | JSONNotFound)
  =>
    try
      let arr = input as JSONArray
      match value
      | let j: JSONValue => arr.update(_idx, j)?
      else
        // _Delete (remove) on array index — not supported, return JSONNotFound
        JSONNotFound
      end
    else
      JSONNotFound
    end

class val _TravCompose is _JSONTraversal
  """
  Sequential composition: navigate _a, then navigate _b within the result.
  """
  let _a: _JSONTraversal
  let _b: _JSONTraversal

  new val create(a: _JSONTraversal, b: _JSONTraversal) =>
    _a = a
    _b = b

  fun apply(v: JSONValue): (JSONValue | JSONNotFound) =>
    match _a(v)
    | let j: JSONValue => _b(j)
    else JSONNotFound
    end

  fun update(
    input: JSONValue,
    value: (JSONValue | _Delete))
    : (JSONValue | JSONNotFound)
  =>
    try
      let intermediate = _a(input) as JSONValue
      let inner_result =
        _b.update(intermediate, value) as JSONValue
      _a.update(input, inner_result)
    else
      JSONNotFound
    end

class val _TravChoice is _JSONTraversal
  """
  Choice: try _a, fall back to _b if JSONNotFound.
  """
  let _a: _JSONTraversal
  let _b: _JSONTraversal

  new val create(a: _JSONTraversal, b: _JSONTraversal) =>
    _a = a
    _b = b

  fun apply(v: JSONValue): (JSONValue | JSONNotFound) =>
    match _a(v)
    | let j: JSONValue => j
    else _b(v)
    end

  fun update(
    input: JSONValue,
    value: (JSONValue | _Delete))
    : (JSONValue | JSONNotFound)
  =>
    match _a(input)
    | let _: JSONValue => _a.update(input, value)
    else _b.update(input, value)
    end
