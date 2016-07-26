use mut = "collections"

type _MapCollisions[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  is Array[_MapLeaf[K, V]] val

type _MapEntry[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  is (_MapNode[K, V] | _MapCollisions[K, V] | _MapLeaf[K, V])

class val _MapNode[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  let _entries: Array[_MapEntry[K, V]] val
  let _nodemap: U32
  let _datamap: U32
  let _level: U8

  new val empty(l: U8) =>
    _entries = recover val
      match l
      | 6 => Array[_MapEntry[K, V]](4)
      else
        Array[_MapEntry[K, V]](32)
      end
    end
    _nodemap = 0
    _datamap = 0
    _level = l

  new val create(es: Array[_MapEntry[K, V]] iso, nm: U32, dm: U32, l: U8) =>
    _entries = consume es
    _nodemap = nm
    _datamap = dm
    _level = l

  fun val apply(hash: U32, key: K): val->V ? =>
    let idx = _mask(hash, _level)
    let i = _array_index(_nodemap, _datamap, idx)
    if i == -1 then error end
    match _entries(i.usize())
    | let l: _MapLeaf[K, V] => l.value
    | let sn: _MapNode[K, V] =>
      sn(hash, key)
    else
      let cn = _entries(i.usize()) as _MapCollisions[K, V]
      for l in cn.values() do
        if l.key == key then return l.value end
      end
      error
    end

  fun val update(value: _MapLeaf[K, V]): (_MapNode[K, V], Bool) ? =>
    let idx = _mask(value.hash, _level)
    let i = _array_index(_nodemap, _datamap, idx)
    if i == -1 then
      let es = recover _entries.clone() end
      let dm = _set_bit(_datamap, idx)
      let i' = _array_index(_nodemap, dm, idx)
      es.insert(i'.usize(), value)
      (create(consume es, _nodemap, dm, _level), true)
    elseif _has_bit(_nodemap, idx) then
      if _level == 6 then
        // insert into collision node
        let es = recover _entries.clone() end
        let cs = _entries(i.usize()) as _MapCollisions[K, V]
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if v.key == value.key then
          // update collision
            cs'(k) = value
            es(i.usize()) = consume cs'
            return (create(consume es, _nodemap, _datamap, _level), false)
          end
        end
        cs'.push(value)
        es(i.usize()) = consume cs'
        (create(consume es, _nodemap, _datamap, _level), true)
      else
        // insert into sub-node
        let sn = _entries(i.usize()) as _MapNode[K, V]
        let es = recover _entries.clone() end
        (let sn', _) = sn.update(value) as (_MapNode[K, V], Bool)
        es(i.usize()) = sn'
        (create(consume es, _nodemap, _datamap, _level), true)
      end
    else
      let old = _entries(i.usize()) as _MapLeaf[K, V]
      if old.key == value.key then
        // update leaf
        let es = recover _entries.clone() end
        es(i.usize()) = value
        (create(consume es, _nodemap, _datamap, _level), false)
      elseif _level == 6 then
        // create collision node
        let cn = recover Array[_MapLeaf[K, V]](2) end
        cn.push(old)
        cn.push(value)
        let es = recover _entries.clone() end
        let dm = _clear_bit(_datamap, idx)
        let nm = _set_bit(_nodemap, idx)
        let i' = _array_index(nm, dm, idx)
        es.delete(i.usize())
        es.insert(i'.usize(), consume cn)
        (create(consume es, nm, dm, _level), true)
      else
        // create new sub-node
        var sn = empty(_level+1)
        (sn, _) = sn.update(old) as (_MapNode[K, V], Bool)
        (sn, _) = sn.update(value) as (_MapNode[K, V], Bool)
        let es = recover _entries.clone() end
        let nm = _set_bit(_nodemap, idx)
        let dm = _clear_bit(_datamap, idx)
        let i' = _array_index(nm, dm, idx)
        es.delete(i.usize())
        es.insert(i'.usize(), sn)
        (create(consume es, nm, dm, _level), true)
      end
    end

  fun val remove(hash: U32, key: K): _MapNode[K, V] ? =>
    let idx = _mask(hash, _level)
    let i = _array_index(_nodemap, _datamap, idx)
    if i == -1 then error end
    if _has_bit(_datamap, idx) then
      var es = recover _entries.clone() end
      es.delete(i.usize())
      create(consume es, _nodemap, _clear_bit(_datamap, idx), _level)
    else
      if _level == 6 then
        let es = recover _entries.clone() end
        let cs = _entries(i.usize()) as _MapCollisions[K, V]
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if v.key == key then
            cs'.delete(k)
            es(i.usize()) = consume cs'
            return create(consume es, _nodemap, _datamap, _level)
          end
        end
        error
      else
        var sn = _entries(i.usize()) as _MapNode[K, V]
        sn = sn.remove(hash, key)
        let es = recover _entries.clone() end
        if (_nodemap.popcount() == 0) and (_datamap.popcount() == 1) then
          for si in mut.Range[U32](0, 32) do
            if _has_bit(_datamap, si) then
              es(i.usize()) = sn._entries(si.usize())
              return create(consume es, _clear_bit(_nodemap, idx), _set_bit(_datamap, idx), _level)
            end
          end
        end
        es(i.usize()) = sn
        create(consume es, _nodemap, _datamap, _level)
      end
    end

  fun _set_bit(bm: U32, i: U32): U32 => bm or (1 << i)

  fun _clear_bit(bm: U32, i: U32): U32 => bm and (not (1 << i))

  fun _has_bit(bm: U32, i: U32): Bool => (bm and (1 << i)) != 0

  fun _mask(hash: U32, l: U8): U32 => (hash >> (5 * l.u32())) and 0x01f

  fun _array_index(nm: U32, dm: U32, idx: U32): U32 =>
    let msk = not(0xffff_ffff << idx)
    let np = msk and nm
    let dp = msk and dm
    let i = (np + dp).popcount()
    if _has_bit(nm, idx) or _has_bit(dm, idx) then i else -1 end

class val _MapLeaf[K: (mut.Hashable val & Equatable[K] val), V: Any val]
  let hash: U32
  let key: K
  let value: V

  new val create(h: U32, k: K, v: V) =>
    hash = h
    key = k
    value = v
