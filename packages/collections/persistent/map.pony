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

  new val _empty() =>
    _root = _MapNode[K, V].empty(0)

  new val _create(r: _MapNode[K, V]) =>
    _root = r

  fun val apply(k: K): val->V ? =>
    """
    Attempt to get the value corresponding to k.
    """
    _root(k.hash().u32(), k)

  fun val size(): USize =>
    """
    Return the amount of key-value pairs in the Map.
    """
    _root.size()

  fun val update(key: K, value: V): Map[K, V] ? =>
    """
    Update the value associated with the provided key.
    """
    _create(_root() =_Leaf[K, V](key.hash().u32(), key, value))

  fun val remove(k: K): Map[K, V] ? =>
    """
    Try to remove the provided key from the Map.
    """
    _create(_root.remove(k.hash().u32(), k))

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

type _MapEntry[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  is (_MapNode[K, V] | Array[_Leaf[K, V]] val | _Leaf[K, V])

class val _MapNode[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  let entries: Array[_MapEntry[K, V]] val
  let nodemap: U32
  let datamap: U32
  let level: U8

  new val empty(l: U8) =>
    entries = recover val
      match l
      | 6 => Array[_MapEntry[K, V]](4)
      else
        Array[_MapEntry[K, V]](32)
      end
    end
    nodemap = 0
    datamap = 0
    level = l

  new val create(es: Array[_MapEntry[K, V]] iso, nm: U32, dm: U32, l: U8) =>
    entries = consume es
    nodemap = nm
    datamap = dm
    level = l

  fun val apply(hash: U32, key: K): val->V ? =>
    let idx = _Hash.mask(hash, level)
    let i = _Hash.array_index(nodemap, datamap, idx)
    if i == -1 then error end
    match entries(i.usize())
    | let l: _Leaf[K, V] => l.value
    | let sn: _MapNode[K, V] =>
      sn(hash, key)
    else
      let cn = entries(i.usize()) as Array[_Leaf[K, V]] val
      for l in cn.values() do
        if l.key == key then return l.value end
      end
      error
    end

  fun val size(): USize =>
    var tot: USize = 0
    for e in entries.values() do
      match e
      | let sn: _MapNode[K, V] => tot = tot + sn.size()
      | let l: _Leaf[K, V] => tot = tot + 1
      | let cn: Array[_Leaf[K, V]] val => tot = tot + cn.size()
      end
    end
    tot

  fun val update(value: _Leaf[K, V]): _MapNode[K, V] ? =>
    let idx = _Hash.mask(value.hash, level)
    let i = _Hash.array_index(nodemap, datamap, idx)
    if i == -1 then
      let es = recover entries.clone() end
      let dm = _Hash.set_bit(datamap, idx)
      let i' = _Hash.array_index(nodemap, dm, idx)
      es.insert(i'.usize(), value)
      create(consume es, nodemap, dm, level)
    elseif _Hash.has_bit(nodemap, idx) then
      if level == 6 then
        // insert into collision node
        let es = recover entries.clone() end
        let cs = entries(i.usize()) as Array[_Leaf[K, V]] val
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if v.key == value.key then
          // update collision
            cs'(k) = value
            es(i.usize()) = consume cs'
            return create(consume es, nodemap, datamap, level)
          end
        end
        cs'.push(value)
        es(i.usize()) = consume cs'
        create(consume es, nodemap, datamap, level)
      else
        // insert into sub-node
        let sn = entries(i.usize()) as _MapNode[K, V]
        let es = recover entries.clone() end
        let sn' = sn() = value
        es(i.usize()) = sn'
        create(consume es, nodemap, datamap, level)
      end
    else
      let old = entries(i.usize()) as _Leaf[K, V]
      if old.key == value.key then
        // update leaf
        let es = recover entries.clone() end
        es(i.usize()) = value
        create(consume es, nodemap, datamap, level)
      elseif level == 6 then
        // create collision node
        let cn = recover Array[_Leaf[K, V]](2) end
        cn.push(old)
        cn.push(value)
        let es = recover entries.clone() end
        let dm = _Hash.clear_bit(datamap, idx)
        let nm = _Hash.set_bit(nodemap, idx)
        let i' = _Hash.array_index(nm, dm, idx)
        es.delete(i.usize())
        es.insert(i'.usize(), consume cn)
        create(consume es, nm, dm, level)
      else
        // create new sub-node
        var sn = empty(level+1)
        sn = sn() = old
        sn = sn() = value
        let es = recover entries.clone() end
        let nm = _Hash.set_bit(nodemap, idx)
        let dm = _Hash.clear_bit(datamap, idx)
        let i' = _Hash.array_index(nm, dm, idx)
        es.delete(i.usize())
        es.insert(i'.usize(), sn)
        create(consume es, nm, dm, level)
      end
    end

  fun val remove(hash: U32, key: K): _MapNode[K, V] ? =>
    let idx = _Hash.mask(hash, level)
    let i = _Hash.array_index(nodemap, datamap, idx)
    if i == -1 then error end
    if _Hash.has_bit(datamap, idx) then
      var es = recover entries.clone() end
      es.delete(i.usize())
      create(consume es, nodemap, _Hash.clear_bit(datamap, idx), level)
    else
      if level == 6 then
        let es = recover entries.clone() end
        let cs = entries(i.usize()) as Array[_Leaf[K, V]] val
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if v.key == key then
            cs'.delete(k)
            es(i.usize()) = consume cs'
            return create(consume es, nodemap, datamap, level)
          end
        end
        error
      else
        var sn = entries(i.usize()) as _MapNode[K, V]
        sn = sn.remove(hash, key)
        let es = recover entries.clone() end
        if (nodemap.popcount() == 0) and (datamap.popcount() == 1) then
          for si in mut.Range[U32](0, 32) do
            if _Hash.has_bit(datamap, si) then
              es(i.usize()) = sn.entries(si.usize())
              return create(consume es, _Hash.clear_bit(nodemap, idx), _Hash.set_bit(datamap, idx), level)
            end
          end
        end
        es(i.usize()) = sn
        create(consume es, nodemap, datamap, level)
      end
    end

class val _Leaf[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  let hash: U32
  let key: K
  let value: V

  new val create(h: U32, k: K, v: V) =>
    hash = h
    key = k
    value = v

primitive _Hash
  fun set_bit(bitMap: U32, i: U32): U32 => bitMap or (1 << i)

  fun clear_bit(bitMap: U32, i: U32): U32 => bitMap and (not (1 << i))

  fun has_bit(bitMap: U32, i: U32): Bool => (bitMap and (1 << i)) != 0

  fun mask(hash: U32, level: U8): U32 => (hash >> (5 * level.u32())) and 0x01f

  fun array_index(nodemap: U32, datamap: U32, idx: U32): U32 =>
    let msk = not(0xffff_ffff << idx)
    let np = msk and nodemap
    let dp = msk and datamap
    let i = (np + dp).popcount()
    if has_bit(nodemap, idx) or has_bit(datamap, idx) then
      i
    else
      -1
    end
