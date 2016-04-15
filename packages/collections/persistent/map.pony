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
    Return the amount of key-value paris in the Map.
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
    let i = _Hash.mask(hash, level)
    if entries(i) is None then
      error
    elseif _Hash.hasBit(dataMap, i) then
      let e = entries(i) as _Leaf[K, V]
      match e.key
      | key => e.value
      else
        error
      end
    elseif _Hash.hasBit(nodeMap, i) then
      let sn = entries(i) as _Node[K, V]
      sn(hash, key)
    else
      let cn = entries(i) as Array[_Leaf[K, V]] val
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
    let i = _Hash.mask(leaf.hash, level)
    if entries(i) is None then
      // insert directly
      let es = recover entries.clone() end
      es.update(i, leaf)
      create(consume es, nodeMap, _Hash.setBit(dataMap, i), level)
    elseif _Hash.hasBit(nodeMap, i) then
      // insert into sub-node
      var sn = entries(i) as _Node[K, V]
      sn = sn.update(leaf)
      let es = recover entries.clone() end
      es.update(i, sn)
      create(consume es, _Hash.setBit(nodeMap, i), dataMap, level)
    elseif _Hash.hasBit(dataMap, i) then
      let old = entries(i) as _Leaf[K, V]
      if old.key == leaf.key then
        // update leaf
        let es = recover entries.clone() end
        es.update(i, leaf)
        create(consume es, nodeMap, dataMap, level)
      elseif level == 6 then
        // create collision node
        let cn = recover iso Array[_Leaf[K, V]](2) end
        cn.push(old)
        cn.push(leaf)
        let es = recover entries.clone() end
        es.update(i, consume cn)
        create(consume es, nodeMap, _Hash.clearBit(dataMap, i), level)
      else
        // create new sub-node
        var sn = empty(level+1)
        sn = sn.update(old)
        sn = sn.update(leaf)
        let es = recover entries.clone() end
        es.update(i, sn)
        create(consume es, _Hash.setBit(nodeMap, i), _Hash.clearBit(dataMap, i), level)
      end
    else
      // insert into collision node
      let es = recover entries.clone() end
      let cs = entries(i) as Array[_Leaf[K, V]] val
      let cs' = recover cs.clone() end
      for (k, v) in cs.pairs() do
        if v.key == leaf.key then
          // update collision
          cs'.update(k, leaf)
          es.update(i, consume cs')
          return create(consume es, nodeMap, dataMap, level)
        end
      end
      cs'.push(leaf)
      es.update(i, consume cs')
      create(consume es, nodeMap, dataMap, level)
    end

  fun val remove(hash: U32, key: K): _Node[K, V] ? =>
    let i = _Hash.mask(hash, level)
    if entries(i) is None then
      error
    elseif _Hash.hasBit(dataMap, i) then
      var es = recover entries.clone() end
      es.update(i, None)
      create(consume es, nodeMap, _Hash.clearBit(dataMap, i), level)
    elseif _Hash.hasBit(nodeMap, i) then
      var sn = entries(i) as _Node[K, V]
      sn = sn.remove(hash, key)
      let es = recover entries.clone() end
      if (nodeMap.popcount() == 0) and (dataMap.popcount() == 1) then
        for ind in mut.Range[U32](0, 32) do
          if _Hash.hasBit(dataMap, ind.usize()) then
            es.update(i, sn.entries(ind.usize()))
            return create(consume es, _Hash.clearBit(nodeMap, i), _Hash.setBit(dataMap, i), level)
          end
        end
      end
      es.update(i, sn)
      create(consume es, nodeMap, dataMap, level)
    else
      let es = recover entries.clone() end
      let cs = entries(i) as Array[_Leaf[K, V]] val
      let cs' = recover cs.clone() end
      for (k, v) in cs.pairs() do
        if v.key == key then
          cs'.delete(k)
          es.update(i, consume cs')
          return create(consume es, nodeMap, dataMap, level)
        end
      end
      error
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
  fun setBit(bitMap: U32, i: USize): U32 => bitMap or (1 << i.u32())
  fun clearBit(bitMap: U32, i: USize): U32 => bitMap and (not (1 << i.u32()))
  fun hasBit(bitMap: U32, i: USize): Bool => (bitMap and (1 << i.u32())) != 0
  fun mask(hash: U32, level: U8): USize => ((hash >> (5 * level.u32())) and 0x01f).usize()
