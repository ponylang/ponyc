class val JSONLens
  """
  Composable, reusable JSON path for reading and modifying nested values.

  Define a lens by chaining key/index steps, then apply it to any document:

  ```pony
  let host_lens = JSONLens("config")("database")("host")

  // Read
  match host_lens.get(doc)
  | let host: String => env.out.print(host)
  | JSONNotFound => env.out.print("no host configured")
  end

  // Modify (returns new document with the change applied)
  match host_lens.set(doc, "newhost.example.com")
  | let updated: JSONValue => // updated doc
  | JSONNotFound => // path didn't exist
  end
  ```
  """

  let _traversal: _JSONTraversal

  new val create() =>
    """
    Create an identity lens (focuses on the root value).
    """
    _traversal = _NoTraversal

  new val _trav(trav': _JSONTraversal) =>
    _traversal = trav'

  fun apply(key_or_index: (String | USize)): JSONLens =>
    """
    Compose a navigation step onto this lens.
    """
    let step: _JSONTraversal =
      match \exhaustive\ key_or_index
      | let k: String => _TravObjKey(k)
      | let i: USize => _TravArrayIndex(i)
      end
    JSONLens._trav(_traversal.compose(step))

  fun get(input: JSONValue): (JSONValue | JSONNotFound) =>
    """
    Apply this lens to read a value.
    """
    _traversal(input)

  fun set(input: JSONValue, value: JSONValue): (JSONValue | JSONNotFound) =>
    """
    Apply this lens to update a value, returning a new root.
    Returns JSONNotFound if the path doesn't exist.
    """
    _traversal.update(input, value)

  fun remove(input: JSONValue): (JSONValue | JSONNotFound) =>
    """
    Apply this lens to remove a value, returning a new root.
    Returns JSONNotFound if the path doesn't exist.
    """
    _traversal.update(input, _Delete)

  fun compose(other: JSONLens): JSONLens =>
    """
    Sequential composition: navigate this lens, then the other.
    """
    JSONLens._trav(_traversal.compose(other._traversal))

  fun or_else(alt: JSONLens): JSONLens =>
    """
    Choice: try this lens, fall back to alt if JSONNotFound.
    """
    JSONLens._trav(_traversal.or_else(alt._traversal))
