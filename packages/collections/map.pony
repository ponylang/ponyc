interface Hashable
  """
  Anything with a hash method is hashable.
  """
  fun box hash(): U64

primitive _MapEmpty
primitive _MapDeleted

// class Identity[A] is Hashable, Comparable[Identity[A] box]
//   var _item: A
//
//   new create(item: A) =>
//     _item = consume item
//
//   fun box hash(): U64 =>
//     Pointer[A].to(_item).hash()
//
//   fun box eq(that: Identity[A] box): Bool =>
//     _item is that._item

interface Hasher[A]
  fun box hash(a: A): U64
  fun box compare(a: A, b: A): Bool

primitive ValueHasher[A: (Hashable box & Comparable[A] box)]
  fun box hash(a: A): U64 => a.hash()
  fun box compare(a: A, b: A): Bool => a == b

// primitive IdentityHasher[A]
//   fun box hash(a: A): U64 => a // pointer hash somehow?
//   fun box compare(a: A, b: A): Bool => a is b

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
    let n = prealloc.next_pow2()
    _array = Array[((Key, Value) | _MapEmpty | _MapDeleted)](n)

    for i in Range(0, n) do
      _array.append(_MapEmpty)
    end

  new from(array: Array[(Key, Value)]) =>
    """
    Create a map from an array of tuples. Because the value may be isolated,
    this removes the tuples from the array, leaving it empty.
    """
    let n = array.size().next_pow2()
    _array = Array[((Key, Value) | _MapEmpty | _MapDeleted)](n)

    for i in Range(0, n) do
      _array.append(_MapEmpty)
    end

    try
      var i = array.size()

      while i > 0 do
        i = i - 1
        let (k, v) = array.delete(i)
        update(consume k, consume v)
      end
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
      _array(index) as (_, this->Value)
    else
      error
    end

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

  fun box next_index(prev: U64 = -1): U64 ? =>
    """
    Given an index, return the next index that has a populated key and value.
    Raise an error if there is no next populated index.
    """
    for i in Range(prev + 1, _array.size()) do
      match _array(i)
      | (let k: this->Key, _) => return i
      end
    end
    error

  fun box index(i: U64): (this->Key, this->Value) ? =>
    """
    Returns the key and value at a given index.
    Raise an error if the index is not populated.
    """
    _array(i) as (this->Key, this->Value)

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
        | (let k: this->Key, _) =>
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

    _array = Array[((Key, Value) | _MapEmpty | _MapDeleted)](new_len)
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
  M: Map[Key, Value] box] is Iterator[M->Key]
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

  fun ref next(): M->Key ? =>
    """
    Returns the next key, or raises an error if there isn't one. If keys are
    added during iteration, this may not return all keys.
    """
    _i = _map.next_index(_i)
    _map.index(_i)._1

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
    _i = _map.next_index(_i)
    _map.index(_i)._2

class MapPairs[
  Key: (Hashable box & Comparable[Key] box),
  Value,
  M: Map[Key, Value] box] is Iterator[(M->Key, M->Value)]
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

  fun ref next(): (M->Key, M->Value) ? =>
    """
    Returns the next entry, or raises an error if there isn't one. If entries
    are added during iteration, this may not return all entries.
    """
    _i = _map.next_index(_i)
    _map.index(_i)
