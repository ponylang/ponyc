primitive _MapEmpty
primitive _MapDeleted

type Map[K: (Hashable box & Comparable[K] box), V] is HashMap[K, V, HashEq[K]]
  """
  This is a map that uses structural equality on the key.
  """

type MapIs[K, V] is HashMap[K, V, HashIs[K]]
  """
  This is a map that uses identity comparison on the key.
  """

class HashMap[K, V, H: HashFunction[K] val]
  """
  A quadratic probing hash map. Resize occurs at a load factor of 0.75. A
  resized map has 2 times the space. The hash function can be plugged in to the
  type to create different kinds of maps.
  """
  var _size: U64 = 0
  var _array: Array[((K, V) | _MapEmpty | _MapDeleted)]

  new create(prealloc: U64 = 8) =>
    """
    Defaults to a prealloc of 8.
    """
    let n = prealloc.next_pow2().max(8)
    _array = _array.create(n)

    for i in Range(0, n) do
      _array.push(_MapEmpty)
    end

  new from(array: Array[(K, V)]) =>
    """
    Create a map from an array of tuples. Because the value may be isolated,
    this removes the tuples from the array, leaving it empty.
    """
    let n = array.size().next_pow2().max(8)
    _array = _array.create(n)

    for i in Range(0, n) do
      _array.push(_MapEmpty)
    end

    try
      var i = array.size()

      while i > 0 do
        i = i - 1
        (let k, let v) = array.delete(i)
        this(consume k) = consume v
      end
    end

  fun size(): U64 =>
    """
    The number of items in the map.
    """
    _size

  fun space(): U64 =>
    """
    The available space in the map. Resize will happen when
    size / space >= 0.75.
    """
    _array.space()

  fun apply(key: this->K!): this->V ? =>
    """
    Gets a value from the map. Raises an error if no such item exists.
    """
    (var index, var found) = _search(key)

    if found then
      _array(index) as (_, this->V)
    else
      error
    end

  fun ref update(key: K, value: V): (V^ | None) =>
    """
    Sets a value in the map. Returns the old value if there was one, otherwise
    returns None. If there was no previous value, this may trigger a resize.
    """
    try
      (let index, let found) = _search(key)

      match _array(index) = (consume key, consume value)
      | (_, let v: V) =>
        return consume v
      else
        _size = _size + 1

        if (_size * 4) > (_array.size() * 3) then
          _resize(_array.size() * 2)
        end
      end
    end
    None

  fun ref remove(key: K!): V^ ? =>
    """
    Delete a value from the map and return it. Raises an error if there was no
    value for the given key.
    """
    try
      (let index, let found) = _search(key)

      if found then
        _size = _size - 1

        match _array(index) = _MapDeleted
        | (_, let v: V) =>
          return consume v
        end
      end
    end
    error

  fun ref add(kv: (K, V)): HashMap[K, V, H]^ =>
    """
    Set a value in the map using +. Return the map, allowing chaining.
    """
    (let k, let v) = consume kv
    this(consume k) = consume v
    this

  fun ref sub(key: K!): HashMap[K, V, H]^ =>
    """
    Remove a value from the map using -. Return the map, allowing chaining.
    """
    try
      remove(key)
    end
    this

  fun next_index(prev: U64 = -1): U64 ? =>
    """
    Given an index, return the next index that has a populated key and value.
    Raise an error if there is no next populated index.
    """
    for i in Range(prev + 1, _array.size()) do
      match _array(i)
      | (_, _) => return i
      end
    end
    error

  fun index(i: U64): (this->K, this->V) ? =>
    """
    Returns the key and value at a given index.
    Raise an error if the index is not populated.
    """
    _array(i) as (this->K, this->V)

  fun ref compact(): HashMap[K, V, H]^ =>
    """
    Minimise the memory used for the map.
    """
    _resize(((_size * 4) / 3).next_pow2().max(8))
    this

  fun ref clear(): HashMap[K, V, H]^ =>
    """
    Remove all entries.
    """
    _size = 0
    _array = _array.create()
    this

  fun _search(key: this->K!): (U64, Bool) =>
    """
    Return a slot number and whether or not it's currently occupied.
    """
    var idx_del = _array.size()
    let mask = idx_del - 1
    let h = H.hash(key)
    var idx = h and mask

    try
      for i in Range(0, _array.size()) do
        let entry = _array(idx)

        match entry
        | (let k: this->K!, _) =>
          if H.eq(k, key) then
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

  fun ref _resize(len: U64) =>
    """
    Change the available space.
    """
    let old = _array
    let old_len = old.size()

    _array = _array.create(len)
    _size = 0

    for i in Range(0, len) do
      _array.push(_MapEmpty)
    end

    try
      for i in Range(0, old_len) do
        match old(i) = _MapDeleted
        | (let k: K, let v: V) =>
          this(consume k) = consume v
        end
      end
    end

  fun keys(): MapKeys[K, V, H, this->HashMap[K, V, H]]^ =>
    """
    Return an iterator over the keys.
    """
    MapKeys[K, V, H, this->HashMap[K, V, H]](this)

  fun values(): MapValues[K, V, H, this->HashMap[K, V, H]]^ =>
    """
    Return an iterator over the values.
    """
    MapValues[K, V, H, this->HashMap[K, V, H]](this)

  fun pairs(): MapPairs[K, V, H, this->HashMap[K, V, H]]^ =>
    """
    Return an iterator over the keys and values.
    """
    MapPairs[K, V, H, this->HashMap[K, V, H]](this)

class MapKeys[K, V, H: HashFunction[K] val, M: HashMap[K, V, H] box] is
  Iterator[M->K]
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

  fun has_next(): Bool =>
    """
    True if it believes there are remaining entries. May not be right if values
    were added or removed from the map.
    """
    _count < _map.size()

  fun ref next(): M->K ? =>
    """
    Returns the next key, or raises an error if there isn't one. If keys are
    added during iteration, this may not return all keys.
    """
    _i = _map.next_index(_i)
    _count = _count + 1
    _map.index(_i)._1

class MapValues[K, V, H: HashFunction[K] val, M: HashMap[K, V, H] box] is
  Iterator[M->V]
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

  fun has_next(): Bool =>
    """
    True if it believes there are remaining entries. May not be right if values
    were added or removed from the map.
    """
    _count < _map.size()

  fun ref next(): M->V ? =>
    """
    Returns the next value, or raises an error if there isn't one. If values
    are added during iteration, this may not return all values.
    """
    _i = _map.next_index(_i)
    _count = _count + 1
    _map.index(_i)._2

class MapPairs[K, V, H: HashFunction[K] val, M: HashMap[K, V, H] box] is
  Iterator[(M->K, M->V)]
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

  fun has_next(): Bool =>
    """
    True if it believes there are remaining entries. May not be right if values
    were added or removed from the map.
    """
    _count < _map.size()

  fun ref next(): (M->K, M->V) ? =>
    """
    Returns the next entry, or raises an error if there isn't one. If entries
    are added during iteration, this may not return all entries.
    """
    _i = _map.next_index(_i)
    _count = _count + 1
    _map.index(_i)
