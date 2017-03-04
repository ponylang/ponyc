use mut = "collections"

type _MapCollisions[K: (mut.Hashable val & Equatable[K]), V: Any #share]
  is Array[_MapLeaf[K, V]] val

type _MapLeaf[K: (mut.Hashable val & Equatable[K]), V: Any #share]
  is (K, V)

type _MapEntry[K: (mut.Hashable val & Equatable[K]), V: Any #share]
  is (_MapNode[K, V] | _MapCollisions[K, V] | _MapLeaf[K, V])

class val _MapNode[K: (mut.Hashable val & Equatable[K]), V: Any #share]
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

  fun val apply(hash: U32, key: K): V ? =>
    let idx = _mask(hash, _level)
    let i = _array_index(_nodemap, _datamap, idx)
    if i == -1 then error end
    match _entries(i.usize_unsafe())
    | (_, let v: V) => v
    | let sn: _MapNode[K, V] => sn(hash, key)
    else
      let cn = _entries(i.usize_unsafe()) as _MapCollisions[K, V]
      for l in cn.values() do
        if l._1 == key then return l._2 end
      end
      error
    end

  fun val update(hash: U32, leaf: _MapLeaf[K, V]): (_MapNode[K, V], Bool) ? =>
    let idx = _mask(hash, _level)
    let i = _array_index(_nodemap, _datamap, idx)
    if i == -1 then
      let es = recover _entries.clone() end
      let dm = _set_bit(_datamap, idx)
      let i' = _array_index(_nodemap, dm, idx)
      es.insert(i'.usize_unsafe(), leaf)
      (create(consume es, _nodemap, dm, _level), true)
    elseif _has_bit(_nodemap, idx) then
      if _level == 6 then
        // insert into collision node
        let es = recover _entries.clone() end
        let cs = _entries(i.usize_unsafe()) as _MapCollisions[K, V]
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if v._1 == leaf._1 then
          // update collision
            cs'(k) = leaf
            es(i.usize_unsafe()) = consume cs'
            return (create(consume es, _nodemap, _datamap, _level), false)
          end
        end
        cs'.push(leaf)
        es(i.usize_unsafe()) = consume cs'
        (create(consume es, _nodemap, _datamap, _level), true)
      else
        // insert into sub-node
        let sn = _entries(i.usize_unsafe()) as _MapNode[K, V]
        let es = recover _entries.clone() end
        (let sn', _) = sn.update(hash, leaf) as (_MapNode[K, V], Bool)
        es(i.usize_unsafe()) = sn'
        (create(consume es, _nodemap, _datamap, _level), true)
      end
    else
      let old = _entries(i.usize_unsafe()) as _MapLeaf[K, V]
      if old._1 == leaf._1 then
        // update leaf
        let es = recover _entries.clone() end
        es(i.usize()) = leaf
        (create(consume es, _nodemap, _datamap, _level), false)
      elseif _level == 6 then
        // create collision node
        let cn = recover Array[_MapLeaf[K, V]](2) end
        cn.push(old)
        cn.push(leaf)
        let es = recover _entries.clone() end
        let dm = _clear_bit(_datamap, idx)
        let nm = _set_bit(_nodemap, idx)
        let i' = _array_index(nm, dm, idx)
        es.delete(i.usize_unsafe())
        es.insert(i'.usize_unsafe(), consume cn)
        (create(consume es, nm, dm, _level), true)
      else
        // create new sub-node
        var sn = empty(_level + 1)
        (sn, _) = sn.update(old._1.hash().u32(), old) as (_MapNode[K, V], Bool)
        (sn, _) = sn.update(hash, leaf) as (_MapNode[K, V], Bool)
        let es = recover _entries.clone() end
        let nm = _set_bit(_nodemap, idx)
        let dm = _clear_bit(_datamap, idx)
        let i' = _array_index(nm, dm, idx)
        es.delete(i.usize_unsafe())
        es.insert(i'.usize_unsafe(), sn)
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
        let cs = _entries(i.usize_unsafe()) as _MapCollisions[K, V]
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if v._1 == key then
            cs'.delete(k)
            es(i.usize_unsafe()) = consume cs'
            return create(consume es, _nodemap, _datamap, _level)
          end
        end
        error
      else
        var sn = _entries(i.usize_unsafe()) as _MapNode[K, V]
        sn = sn.remove(hash, key)
        let es = recover _entries.clone() end
        if (_nodemap.popcount() == 0) and (_datamap.popcount() == 1) then
          for si in mut.Range[U32](0, 32) do
            if _has_bit(_datamap, si) then
              es(i.usize_unsafe()) = sn._entries(si.usize_unsafe())
              return create(consume es, _clear_bit(_nodemap, idx),
                _set_bit(_datamap, idx), _level)
            end
          end
        end
        es(i.usize_unsafe()) = sn
        create(consume es, _nodemap, _datamap, _level)
      end
    end

  fun _set_bit(bm: U32, i: U32): U32 => bm or (U32(1) <<~ i)

  fun _clear_bit(bm: U32, i: U32): U32 => bm and (not (U32(1) <<~ i))

  fun _has_bit(bm: U32, i: U32): Bool => (bm and (U32(1) <<~ i)) != 0

  fun _mask(hash: U32, l: U8): U32 =>
    (hash >> (U32(5) *~ l.u32_unsafe())) and 0x01f

  fun _array_index(nm: U32, dm: U32, idx: U32): U32 =>
    let msk = not(0xffff_ffff << idx)
    let np = msk and nm
    let dp = msk and dm
    let i = (np + dp).popcount()
    if _has_bit(nm, idx) or _has_bit(dm, idx) then i else -1 end

  fun entries(): Array[_MapEntry[K, V]] val => _entries
