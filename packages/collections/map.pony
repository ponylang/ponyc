primitive _MapEmpty
primitive _MapDeleted

type Map[K: (Hashable #read & Equatable[K] #read), V] is
  HashMap[K, V, HashEq[K]]
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
  var _size: USize = 0
  var _array: Array[((K, V) | _MapEmpty | _MapDeleted)]

  new create(prealloc: USize = 6) =>
    """
    Create an array with space for prealloc elements without triggering a
    resize. Defaults to 6.
    """
    let len = (prealloc * 4) / 3
    let n = len.max(8).next_pow2()
    _array = _array.init(_MapEmpty, n)

  fun size(): USize =>
    """
    The number of items in the map.
    """
    _size

  fun space(): USize =>
    """
    The available space in the map. Resize will happen when
    size / space >= 0.75.
    """
    _array.space()

  fun apply(key: box->K!): this->V ? =>
    """
    Gets a value from the map. Raises an error if no such item exists.
    """
    (let i, let found) = _search(key)

    if found then
      _array(i) as (_, this->V)
    else
      error
    end

  fun ref update(key: K, value: V): (V^ | None) =>
    """
    Sets a value in the map. Returns the old value if there was one, otherwise
    returns None. If there was no previous value, this may trigger a resize.
    """
    try
      (let i, let found) = _search(key)

      let k = if found then
        _array(i) as (K^, _)
      else
        consume key
      end
      match _array(i) = (consume k, consume value)
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

  fun ref upsert(key: K, value: V, f: {(V, V): V^} box): HashMap[K, V, H]^ =>
    """
    Combines a provided value with the current value for the provided key
    using the provided function. If the provided key has not been added to
    the map yet, it sets its value to the provided value.

    As a simple example, say we had a map with U64 values and we wanted to
    add 4 to the current value for key "test", which let's say is currently 2.
    We call

    m.upsert("test", 4, lambda(x: U64, y: U64): U64 => x + y end)

    This changes the value associated with "test" to 6.

    If we have not yet added the key "new-key" to the map and we call

    m.upsert("new-key", 4, lambda(x: U64, y: U64): U64 => x + y end)

    then "new-key" is added to the map with a value of 4.
    """

    (let i, let found) = _search(key)

    try
      if found then
        (let pkey, let pvalue) = (_array(i) = _MapEmpty) as (K^, V^)
        _array(i) = (consume pkey, f(consume pvalue, consume value))
      else
        _array(i) = (consume key, consume value)
        _size = _size + 1

        if (_size * 4) > (_array.size() * 3) then
          _resize(_array.size() * 2)
        end
      end
    end
    this

  fun ref insert(key: K, value: V): V ? =>
    """
    Set a value in the map. Returns the new value, allowing reuse.
    """
    try
      (let i, let found) = _search(key)
      let key' = key
      _array(i) = (consume key, consume value)

      if not found then
        _size = _size + 1

        if (_size * 4) > (_array.size() * 3) then
          _resize(_array.size() * 2)
          return this(key')
        end
      end

      _array(i) as (_, V)
    else
      // This is unreachable, since index will never be out-of-bounds.
      error
    end

  fun ref remove(key: box->K!): (K^, V^) ? =>
    """
    Delete a value from the map and return it. Raises an error if there was no
    value for the given key.
    """
    try
      (let i, let found) = _search(key)

      if found then
        _size = _size - 1

        match _array(i) = _MapDeleted
        | (let k: K, let v: V) =>
          return (consume k, consume v)
        end
      end
    end
    error

  fun get_or_else(key: box->K!, alt: this->V): this->V =>
    """
    Get the value associated with provided key if present. Otherwise,
    return the provided alternate value.
    """
    try
      apply(key)
    else
      consume alt
    end

  fun contains(k: box->K!): Bool =>
    """
    Checks whether the map contains the key k
    """
    (_, let found) = _search(k)
    found

  fun ref concat(iter: Iterator[(K^, V^)]): HashMap[K, V, H]^ =>
    """
    Add K, V pairs from the iterator to the map.
    """
    for (k, v) in iter do
      this(consume k) = consume v
    end
    this

  fun add[H2: HashFunction[this->K!] val = H](key: this->K!, value: this->V!):
    HashMap[this->K!, this->V!, H2]^
  =>
    """
    This with the new (key, value) mapping.
    """
    let r = clone[H2]()
    r(key) = value
    r

  fun sub[H2: HashFunction[this->K!] val = H](key: this->K!, value: this->V!):
    HashMap[this->K!, this->V!, H2]^
  =>
    """
    This without the given key.
    """
    let r = clone[H2]()
    try r.remove(key) end
    r

  fun next_index(prev: USize = -1): USize ? =>
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

  fun index(i: USize): (this->K, this->V) ? =>
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

  fun clone[H2: HashFunction[this->K!] val = H]():
    HashMap[this->K!, this->V!, H2]^
  =>
    """
    Create a clone. The key and value types may be different due to aliasing
    and viewpoint adaptation.
    """
    let r = HashMap[this->K!, this->V!, H2](_size)

    for (k, v) in pairs() do
      r(k) = v
    end
    r

  fun ref clear(): HashMap[K, V, H]^ =>
    """
    Remove all entries.
    """
    _size = 0
    // Our default prealloc of 6 corresponds to an array alloc size of 8.
    let n: USize = 8
    _array = _array.init(_MapEmpty, n)
    this

  fun _search(key: box->K!): (USize, Bool) =>
    """
    Return a slot number and whether or not it's currently occupied.
    """
    var idx_del = _array.size()
    let mask = idx_del - 1
    let h = H.hash(key).usize()
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
            idx_del = idx
          end
        end

        idx = (h + ((i + (i * i)) / 2)) and mask
      end
    end

    (idx_del, false)

  fun ref _resize(len: USize) =>
    """
    Change the available space.
    """
    let old = _array
    let old_len = old.size()

    _array = _array.init(_MapEmpty, len)
    _size = 0

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

class MapKeys[K, V, H: HashFunction[K] val, M: HashMap[K, V, H] #read] is
  Iterator[M->K]
  """
  An iterator over the keys in a map.
  """
  let _map: M
  var _i: USize = -1
  var _count: USize = 0

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

class MapValues[K, V, H: HashFunction[K] val, M: HashMap[K, V, H] #read] is
  Iterator[M->V]
  """
  An iterator over the values in a map.
  """
  let _map: M
  var _i: USize = -1
  var _count: USize = 0

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

class MapPairs[K, V, H: HashFunction[K] val, M: HashMap[K, V, H] #read] is
  Iterator[(M->K, M->V)]
  """
  An iterator over the keys and values in a map.
  """
  let _map: M
  var _i: USize = -1
  var _count: USize = 0

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
