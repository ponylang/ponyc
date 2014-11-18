interface Hashable
  """
  Anything with a hash method is hashable.
  """
  fun box hash(): U64

primitive _MapEmpty is Comparable[_MapEmpty]
primitive _MapDeleted is Comparable[_MapDeleted]

class Map[Key: (Hashable box & Comparable[Key] box), Value]
  """
  A quadratic probing hash map. Resize occurs at a load factor of 0.75. A
  resized map has 2 times the space.
  """
  var _size: U64 = 0
  var _array: Array[((Key, Value) | _MapEmpty | _MapDeleted)]

  new create(prealloc: U64 = 8) =>
    """
    Defaults to a prealloc of 8.
    """
    var n = prealloc.next_pow2()
    _array = Array[((Key, Value) | _MapEmpty | _MapDeleted)].prealloc(n)

    for i in Range(0, n) do
      _array.append(_MapEmpty)
    end

  fun box size(): U64 =>
    """
    The number of items in the map.
    """
    _size

  fun box space(): U64 =>
    """
    The available space in the map. Resize will happen when
    size / space >= 0.75.
    """
    _array.space()

  fun box apply(key: Key): this->Value ? =>
    """
    Gets a value from the map. Raises an error if no such item exists.
    """
    var (index, found) = _search(key)

    if found then
      match _array(index)
      | (_, let v: this->Value!) =>
        return consume v
      end
    end
    error

  fun ref update(key: Key, value: Value): (Value^ | None) =>
    """
    Sets a value in the map. Returns the old value if there was one, otherwise
    returns None. If there was no previous value, this may trigger a resize.
    """
    try
      let (index, found) = _search(key)
      let prev = _array(index) = (key, consume value)

      match consume prev
      | (_, let v: Value) =>
        return consume v
      else
        _size = _size + 1

        if (_size * 4) > (_array.size() * 3) then
          _resize()
        end
      end
    end
    None

  fun ref remove(key: Key): Value^ ? =>
    """
    Delete a value from the map and returns it. Raises an error if there was no
    value for the given key.
    """
    try
      let (index, found) = _search(key)

      if found then
        let prev = _array(index) = _MapDeleted
        _size = _size - 1

        match consume prev
        | (_, let v: Value) =>
          return consume v
        end
      end
    end
    error

  fun box index(idx: U64 = -1): (U64, Key, this->Value) ? =>
    """
    Given an index, return the next index that has a populated key and value.
    Raise an error if there is no next populated index.
    """
    for i in Range(idx + 1, _array.size()) do
      let entry = _array(i)

      match entry
      | (let k: Key!, let v: this->Value!) =>
        return (i, consume k, consume v)
      end
    end
    error

  fun box _search(key: Key): (U64, Bool) =>
    """
    Return a slot number and whether or not it's currently occupied.
    """
    var idx_del = _array.size()
    let mask = idx_del - 1
    let h = key.hash()
    var idx = h and mask

    try
      for i in Range(0, _array.size()) do
        let entry = _array(idx)

        match entry
        | (let k: Key, _) =>
          if k == key then
            return (idx, true)
          end
        | _MapEmpty =>
          if idx_del <= mask then
            return (idx_del, false)
          else
            return (idx, false)
          end
        | _MapDeleted =>
          if idx_del > mask then
            idx_del = i
          end
        end

        idx = (h + ((i + (i * i)) / 2)) and mask
      end
    end

    (idx_del, false)

  fun ref _resize() =>
    """
    Double the available space.
    """
    let _old = _array
    let old_len = _old.size()
    let new_len = old_len * 2

    _array = Array[((Key, Value) | _MapEmpty | _MapDeleted)].prealloc(new_len)
    _size = 0

    for i in Range(0, new_len) do
      _array.append(_MapEmpty)
    end

    try
      for i in Range(0, old_len) do
        let entry = _old(i) = _MapDeleted

        match consume entry
        | (let k: Key, let v: Value) =>
          this(k) = consume v
        end
      end
    end

  fun box keys(): MapKeys[Key, Value, this->Map[Key, Value]]^ =>
    """
    Return an iterator over the keys.
    """
    MapKeys[Key, Value, this->Map[Key, Value]](this)

  fun box values(): MapValues[Key, Value, this->Map[Key, Value]]^ =>
    """
    Return an iterator over the values.
    """
    MapValues[Key, Value, this->Map[Key, Value]](this)

  fun box pairs(): MapPairs[Key, Value, this->Map[Key, Value]]^ =>
    """
    Return an iterator over the keys and values.
    """
    MapPairs[Key, Value, this->Map[Key, Value]](this)

class MapKeys[
  Key: (Hashable box & Comparable[Key] box),
  Value,
  M: Map[Key, Value] box] is Iterator[Key]
  """
  An iterator over the keys in a map.
  """
  let _map: M
  var _i: U64 = -1
  var _count: U64 = 0

  new create(map: M) =>
    """
    Creates an iterator for the given map.
    """
    _map = map

  fun box has_next(): Bool =>
    """
    True if it believes there are remaining entries. May not be right if values
    were added or removed from the map.
    """
    _count < _map.size()

  fun ref next(): Key ? =>
    """
    Returns the next key, or raises an error if there isn't one. If keys are
    added during iteration, this may not return all keys.
    """
    (_i, var k: Key, _) = _map.index(_i)
    _count = _count + 1
    k

class MapValues[
  Key: (Hashable box & Comparable[Key] box),
  Value,
  M: Map[Key, Value] box] is Iterator[M->Value]
  """
  An iterator over the values in a map.
  """
  let _map: M
  var _i: U64 = -1
  var _count: U64 = 0

  new create(map: M) =>
    """
    Creates an iterator for the given map.
    """
    _map = map

  fun box has_next(): Bool =>
    """
    True if it believes there are remaining entries. May not be right if values
    were added or removed from the map.
    """
    _count < _map.size()

  fun ref next(): M->Value ? =>
    """
    Returns the next value, or raises an error if there isn't one. If values are
    added during iteration, this may not return all values.
    """
    (_i, _, var v: M->Value!) = _map.index(_i)
    _count = _count + 1
    consume v

class MapPairs[
  Key: (Hashable box & Comparable[Key] box),
  Value,
  M: Map[Key, Value] box] is Iterator[(Key, M->Value)]
  """
  An iterator over the keys and values in a map.
  """
  let _map: M
  var _i: U64 = -1
  var _count: U64 = 0

  new create(map: M) =>
    """
    Creates an iterator for the given map.
    """
    _map = map

  fun box has_next(): Bool =>
    """
    True if it believes there are remaining entries. May not be right if values
    were added or removed from the map.
    """
    _count < _map.size()

  fun ref next(): (Key, M->Value) ? =>
    """
    Returns the next entry, or raises an error if there isn't one. If entries
    are added during iteration, this may not return all entries.
    """
    (_i, var k: Key, var v: M->Value!) = _map.index(_i)
    _count = _count + 1
    (k, consume v)
