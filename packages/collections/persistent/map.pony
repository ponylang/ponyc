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
      m = m.update(pair._1, pair._2)
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
  let m2 = m1.update("a", 5) // {a: 5}
  let m3 = m2.update("b", 10) // {a: 5, b: 10}
  let m4 = m3.remove("a") // {b: 10}
  // You can create a new map from key value pairs.
  let map = Maps.from[String,U32]([("a", 2), ("b", 3)]) // {a: 2, b: 3}
  ```
  """
  let _root: _Node[K, V]

  new val _empty() =>
    _root = _Node[K, V].empty(0)

  new val _create(r: _Node[K, V]) =>
    _root = r

  fun val apply(k: K): val->V ? =>
    """
    Attempt to get the value corresponding to k.
    """
    _root(k.hash().u32(), k)

  fun val size(): U64 =>
    """
    Return the amount of key-value pairs in the Map.
    """
    _root.size()

  fun val update(k: K, v: V): Map[K, V] ? =>
    """
    Update the value associated with the provided key.
    """
    _create(_root.update(_Leaf[K, V](k.hash().u32(), k, v)))

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

type _Entry[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  is (_Node[K, V] | Array[_Leaf[K, V]] val | _Leaf[K, V] | None)

class val _Node[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  let entries: Array[_Entry[K, V]] val
  let nodeMap: U32
  let dataMap: U32
  let level: U8

  new val empty(l: U8) =>
    entries = recover val
      match l
      | 6 => Array[_Entry[K, V]].init(None, 4)
      else
        Array[_Entry[K, V]].init(None, 32)
      end
    end
    nodeMap = 0
    dataMap = 0
    level = l

  new val create(es: Array[_Entry[K, V]] iso, nm: U32, dm: U32, l: U8) =>
    entries = consume es
    nodeMap = nm
    dataMap = dm
    level = l

  fun val apply(hash: U32, key: K): val->V ? =>
    let idx = _Hash.mask(hash, level)
    let i = _Hash.arrayIndex(nodeMap, dataMap, idx)
    if i == -1 then error end
    match entries(i.usize())
    | let l: _Leaf[K, V] => l.value
    | let sn: _Node[K, V] =>
      sn(hash, key)
    else
      let cn = entries(i.usize()) as Array[_Leaf[K, V]] val
      for l in cn.values() do
        if l.key == key then return l.value end
      end
      error
    end

  fun val size(): U64 =>
    var tot: U64 = 0
    for e in entries.values() do
      match e
      | let sn: _Node[K, V] => tot = tot + sn.size()
      | let l: _Leaf[K, V] => tot = tot + 1
      end
    end
    tot

  fun val update(leaf: _Leaf[K, V]): _Node[K, V] ? =>
    let idx = _Hash.mask(leaf.hash, level)
    let i = _Hash.arrayIndex(nodeMap, dataMap, idx)
    if i == -1 then
      let es = recover entries.clone() end
      let dm = _Hash.setBit(dataMap, idx)
      let i' = _Hash.arrayIndex(nodeMap, dm, idx)
      es.insert(i'.usize(), leaf)
      create(consume es, nodeMap, dm, level)
    elseif _Hash.hasBit(nodeMap, idx) then
      if level == 6 then
        // insert into collision node
        let es = recover entries.clone() end
        let cs = entries(i.usize()) as Array[_Leaf[K, V]] val
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if v.key == leaf.key then
          // update collision
            cs'.update(k, leaf)
            es.update(i.usize(), consume cs')
            return create(consume es, nodeMap, dataMap, level)
          end
        end
        cs'.push(leaf)
        es.update(i.usize(), consume cs')
        create(consume es, nodeMap, dataMap, level)
      else
        // insert into sub-node
        let sn = entries(i.usize()) as _Node[K, V]
        let es = recover entries.clone() end
        let sn' = sn.update(leaf)
        es.update(i.usize(), sn')
        create(consume es, nodeMap, dataMap, level)
      end
    else
      let old = entries(i.usize()) as _Leaf[K, V]
      if old.key == leaf.key then
        // update leaf
        let es = recover entries.clone() end
        es.update(i.usize(), leaf)
        create(consume es, nodeMap, dataMap, level)
      elseif level == 6 then
        // create collision node
        let cn = recover Array[_Leaf[K, V]](2) end
        cn.push(old)
        cn.push(leaf)
        let es = recover entries.clone() end
        let dm = _Hash.clearBit(dataMap, idx)
        let nm = _Hash.setBit(nodeMap, idx)
        let i' = _Hash.arrayIndex(nm, dm, idx)
        es.delete(i.usize())
        es.insert(i'.usize(), consume cn)
        create(consume es, nm, dm, level)
      else
        // create new sub-node
        var sn = empty(level+1)
        sn = sn.update(old)
        sn = sn.update(leaf)
        let es = recover entries.clone() end
        let nm = _Hash.setBit(nodeMap, idx)
        let dm = _Hash.clearBit(dataMap, idx)
        let i' = _Hash.arrayIndex(nm, dm, idx)
        es.delete(i.usize())
        es.insert(i'.usize(), sn)
        create(consume es, nm, dm, level)
      end
    end

  fun val remove(hash: U32, key: K): _Node[K, V] ? =>
    let idx = _Hash.mask(hash, level)
    let i = _Hash.arrayIndex(nodeMap, dataMap, idx)
    if i == -1 then error end
    if _Hash.hasBit(dataMap, idx) then
      var es = recover entries.clone() end
      es.delete(i.usize())
      create(consume es, nodeMap, _Hash.clearBit(dataMap, idx), level)
    else
      if level == 6 then
        let es = recover entries.clone() end
        let cs = entries(i.usize()) as Array[_Leaf[K, V]] val
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if v.key == key then
            cs'.delete(k)
            es.update(i.usize(), consume cs')
            return create(consume es, nodeMap, dataMap, level)
          end
        end
        error
      else
        var sn = entries(i.usize()) as _Node[K, V]
        sn = sn.remove(hash, key)
        let es = recover entries.clone() end
        if (nodeMap.popcount() == 0) and (dataMap.popcount() == 1) then
          for si in mut.Range[U32](0, 32) do
            if _Hash.hasBit(dataMap, si) then
              es.update(i.usize(), sn.entries(si.usize()))
              return create(consume es, _Hash.clearBit(nodeMap, idx), _Hash.setBit(dataMap, idx), level)
            end
          end
        end
        es.update(i.usize(), sn)
        create(consume es, nodeMap, dataMap, level)
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
  fun setBit(bitMap: U32, i: U32): U32 => bitMap or (1 << i)

  fun clearBit(bitMap: U32, i: U32): U32 => bitMap and (not (1 << i))

  fun hasBit(bitMap: U32, i: U32): Bool => (bitMap and (1 << i)) != 0

  fun mask(hash: U32, level: U8): U32 => (hash >> (5 * level.u32())) and 0x01f

  fun arrayIndex(nodeMap: U32, dataMap: U32, idx: U32): U32 =>
    let msk = not(0xffff_ffff << idx)
    let np = msk and nodeMap
    let dp = msk and dataMap
    let i = (np + dp).popcount()
    if hasBit(nodeMap, idx) or hasBit(dataMap, idx) then
      i
    else
      -1
    end
