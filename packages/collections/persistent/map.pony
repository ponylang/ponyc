use mut = "collections"

type Map[K: (mut.Hashable val & Equatable[K]), V: Any #share] is
  HashMap[K, V, mut.HashEq[K]]
  """
  A map that uses structural equality on the key.
  """

type MapIs[K: Any #share, V: Any #share] is mut.HashMap[K, V, mut.HashIs[K]]
  """
  A map that uses identity comparison on the key.
  """

class val HashMap[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  """
  A persistent map based on the Compressed Hash Array Mapped Prefix-tree from
  'Optimizing Hash-Array Mapped Tries for Fast and Lean Immutable JVM
  Collections' by Michael J. Steindorfer and Jurgen J. Vinju

  ## Usage
  ```pony
  let empty = Map[String,U32] // {}
  // Update returns a new map with the provided key set
  // to the provided value. The old map is unchanged.
  let m2 = m1("a") = 5 // {a: 5}
  let m3 = m2("b") = 10 // {a: 5, b: 10}
  let m4 = m3.remove("a") // {b: 10}
  // You can create a new map from key value pairs.
  let m5 = Map[String,U32].concat([("a", 2), ("b", 3)].values()) // {a: 2, b: 3}
  ```
  """
  let _root: _MapNode[K, V, H]
  let _size: USize

  new val create() =>
    _root = _MapNode[K, V, H].empty(0)
    _size = 0

  new val _create(r: _MapNode[K, V, H], s: USize) =>
    _root = r
    _size = s

  fun val apply(k: K): val->V ? =>
    """
    Attempt to get the value corresponding to k.
    """
    _root(H.hash(k).u32(), k)?

  fun val size(): USize =>
    """
    Return the amount of key-value pairs in the Map.
    """
    _size

  fun val update(key: K, value: val->V): HashMap[K, V, H] =>
    """
    Update the value associated with the provided key.
    """
    (let r, let insertion) =
      try
        _root.update(H.hash(key).u32(), (key, value))?
      else
        (_root, false) // should not occur
      end
    let s = if insertion then _size + 1 else _size end
    _create(r, s)

  fun val remove(k: K): HashMap[K, V, H] ? =>
    """
    Try to remove the provided key from the Map.
    """
    _create(_root.remove(H.hash(k).u32(), k)?, _size - 1)

  fun val get_or_else(k: K, alt: val->V): val->V =>
    """
    Get the value associated with provided key if present. Otherwise,
    return the provided alternate value.
    """
    try
      apply(k)?
    else
      alt
    end

  fun val contains(k: K): Bool =>
    """
    Check whether the node contains the provided key.
    """
    try
      apply(k)?
      true
    else
      false
    end

  fun val concat(iter: Iterator[(val->K, val->V)]): HashMap[K, V, H] =>
    """
    Add the K, V pairs from the given iterator to the map.
    """
    var m = this
    for (k, v) in iter do
      m = m.update(k, v)
    end
    m

  fun val keys(): MapKeys[K, V, H] => MapKeys[K, V, H](this)

  fun val values(): MapValues[K, V, H] => MapValues[K, V, H](this)

  fun val pairs(): MapPairs[K, V, H] => MapPairs[K, V, H](this)

  fun _root_node(): _MapNode[K, V, H] => _root

class MapKeys[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  embed _pairs: MapPairs[K, V, H]

  new create(m: HashMap[K, V, H]) => _pairs = MapPairs[K, V, H](m)

  fun has_next(): Bool => _pairs.has_next()

  fun ref next(): K ? => _pairs.next()?._1

class MapValues[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  embed _pairs: MapPairs[K, V, H]

  new create(m: HashMap[K, V, H]) => _pairs = MapPairs[K, V, H](m)

  fun has_next(): Bool => _pairs.has_next()

  fun ref next(): val->V ? => _pairs.next()?._2

class MapPairs[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  embed _path: Array[_MapNode[K, V, H]]
  embed _idxs: Array[USize]
  var _i: USize = 0
  let _size: USize
  var _ci: USize = 0

  new create(m: HashMap[K, V, H]) =>
    _size = m.size()
    _path = Array[_MapNode[K, V, H]]
    _path.push(m._root_node())
    _idxs = Array[USize]
    _idxs.push(0)

  fun has_next(): Bool => _i < _size

  fun ref next(): (K, val->V) ? =>
    (let n, let i) = _cur()?
    if i >= n.entries().size() then
      _backup()?
      return next()?
    end
    match n.entries()(i)?
    | let l: _MapLeaf[K, V, H] =>
      _inc_i()?
      _i = _i + 1
      l
    | let sn: _MapNode[K, V, H] =>
      _push(sn)
      next()?
    | let cs: _MapCollisions[K, V, H] =>
      if _ci < cs.size() then
        let l = cs(_ci)?
        _ci = _ci + 1
        _i = _i + 1
        l
      else
        _ci = 0
        _inc_i()?
        next()?
      end
    end

  fun ref _push(n: _MapNode[K, V, H]) =>
    _path.push(n)
    _idxs.push(0)

  fun ref _backup() ? =>
    _path.pop()?
    _idxs.pop()?
    _inc_i()?

  fun ref _inc_i() ? =>
    let i = _idxs.size() - 1
    _idxs(i)? = _idxs(i)? + 1

  fun _cur(): (_MapNode[K, V, H], USize) ? =>
    let i = _idxs.size() - 1
    (_path(i)?, _idxs(i)?)
