use mut = "collections"

primitive Maps
  fun val empty[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  (): Map[K, V] =>
    """
    Return an empty map.
    """
    Map[K, V]._empty()

  fun val from[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  (pairs: Array[(K, val->V)]): Map[K, V] ? =>
    """
    Return a map containing the provided key-value pairs.
    """
    var m = Map[K, V]._empty()
    for pair in pairs.values() do
      m = m(pair._1) = pair._2
    end
    m

class val Map[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  """
  A persistent map based on the Compressed Hash Array Mapped Prefix-tree from
  'Optimizing Hash-Array Mapped Tries for Fast and Lean Immutable JVM
  Collections' by Michael J. Steindorfer and Jurgen J. Vinju

  ## Usage
  ```
  let empty: Map[String,U32] = Maps.empty[String,U32]() // {}
  // Update returns a new map with the provided key set
  // to the provided value. The old map is unchanged.
  let m2 = m1("a") = 5 // {a: 5}
  let m3 = m2("b") = 10 // {a: 5, b: 10}
  let m4 = m3.remove("a") // {b: 10}
  // You can create a new map from key value pairs.
  let map = Maps.from[String,U32]([("a", 2), ("b", 3)]) // {a: 2, b: 3}
  ```
  """
  let _root: _MapNode[K, V]
  let _size: USize

  new val _empty() =>
    _root = _MapNode[K, V].empty(0)
    _size = 0

  new val _create(r: _MapNode[K, V], s: USize) =>
    _root = r
    _size = s

  fun val apply(k: K): val->V ? =>
    """
    Attempt to get the value corresponding to k.
    """
    _root(k.hash().u32(), k)

  fun val size(): USize =>
    """
    Return the amount of key-value pairs in the Map.
    """
    _size

  fun val update(key: K, value: V): Map[K, V] ? =>
    """
    Update the value associated with the provided key.
    """
    (let r, let insertion) =
      _root.update(_MapLeaf[K, V](key.hash().u32(), key, value))
    let s = if insertion then _size+1 else _size end
    _create(r, s)

  fun val remove(k: K): Map[K, V] ? =>
    """
    Try to remove the provided key from the Map.
    """
    _create(_root.remove(k.hash().u32(), k), _size-1)

  fun val get_or_else(k: K, alt: val->V): val->V =>
    """
    Get the value associated with provided key if present. Otherwise,
    return the provided alternate value.
    """
    try
      apply(k)
    else
      alt
    end

  fun val contains(k: K): Bool =>
    """
    Check whether the node contains the provided key.
    """
    try
      apply(k)
      true
    else
      false
    end

  fun val keys(): MapKeys[K, V] => MapKeys[K, V](this)

  fun val values(): MapValues[K, V] => MapValues[K, V](this)

  fun val pairs(): MapPairs[K, V] =>
    MapPairs[K, V](this)

  fun _root_node(): _MapNode[K, V] => _root

class MapKeys[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  embed _pairs: MapPairs[K, V]

  new create(m: Map[K, V]) => _pairs = MapPairs[K, V](m)

  fun has_next(): Bool => _pairs.has_next()

  fun ref next(): K ? => _pairs.next()._1

class MapValues[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  embed _pairs: MapPairs[K, V]

  new create(m: Map[K, V]) => _pairs = MapPairs[K, V](m)

  fun has_next(): Bool => _pairs.has_next()

  fun ref next(): V ? => _pairs.next()._2

class MapPairs[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  embed _path: Array[_MapNode[K, V]]
  embed _idxs: Array[USize]
  var _i: USize = 0
  let _size: USize
  var _ci: USize = 0

  new create(m: Map[K, V]) =>
    _size = m.size()
    _path = Array[_MapNode[K, V]]
    _path.push(m._root_node())
    _idxs = Array[USize]
    _idxs.push(0)

  fun has_next(): Bool => _i < _size

  fun ref next(): (K, V) ? =>
    (let n, let i) = _cur()
    if i >= n.entries().size() then
      _backup()
      return next()
    end
    match n.entries()(i)
    | let l: _MapLeaf[K, V] =>
      _inc_i()
      _i = _i + 1
      (l.key, l.value)
    | let sn: _MapNode[K, V] =>
      _push(sn)
      next()
    | let cs: _MapCollisions[K, V] =>
      if _ci < cs.size() then
        let l = cs(_ci)
        _ci = _ci + 1
        _i = _i + 1
        (l.key, l.value)
      else
        _ci = 0
        _inc_i()
        next()
      end
    else error
    end

  fun ref _push(n: _MapNode[K, V]) =>
    _path.push(n)
    _idxs.push(0)

  fun ref _backup() ? =>
    _path.pop()
    _idxs.pop()
    _inc_i()

  fun ref _inc_i() ? =>
    let i = _idxs.size() - 1
    _idxs(i) = _idxs(i) + 1

  fun _cur(): (_MapNode[K, V], USize) ? =>
    let i = _idxs.size() - 1
    (_path(i), _idxs(i))
