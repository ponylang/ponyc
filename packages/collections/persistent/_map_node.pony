use mut = "collections"

type _MapCollisions[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  is Array[_MapLeaf[K, V, H]] val

type _MapLeaf[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  is (K, V)

type _MapEntry[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  is (_MapNode[K, V, H] | _MapCollisions[K, V, H] | _MapLeaf[K, V, H])

class val _MapNode[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  let _entries: Array[_MapEntry[K, V, H]] val
  let _nodemap: U32
  let _datamap: U32
  let _level: U8

  new val empty(l: U8) =>
    _entries = recover val
      match l
      | 6 => Array[_MapEntry[K, V, H]](4)
      else
        Array[_MapEntry[K, V, H]]
      end
    end
    _nodemap = 0
    _datamap = 0
    _level = l

  new val create(es: Array[_MapEntry[K, V, H]] iso, nm: U32, dm: U32, l: U8) =>
    _entries = consume es
    _nodemap = nm
    _datamap = dm
    _level = l

  fun val apply(hash: U32, key: K): V ? =>
    let idx = _Bits.mask(hash, _level)
    let i = _compressed_index(_nodemap, _datamap, idx)
    if i == -1 then error end
    match _entries(i.usize_unsafe())?
    | (_, let v: V) => v
    | let sn: _MapNode[K, V, H] => sn(hash, key)?
    | let cn: _MapCollisions[K, V, H] =>
      for l in cn.values() do
        if H.eq(l._1, key) then return l._2 end
      end
      error
    end

  fun val update(
    hash: U32,
    leaf: _MapLeaf[K, V, H])
    : (_MapNode[K, V, H], Bool) ?
  =>
    let idx = _Bits.mask(hash, _level)
    let i = _compressed_index(_nodemap, _datamap, idx)
    if i == -1 then
      let es = recover _entries.clone() end
      let dm = _Bits.set_bit(_datamap, idx)
      let i' = _compressed_index(_nodemap, dm, idx)
      es.insert(i'.usize_unsafe(), leaf)?
      (create(consume es, _nodemap, dm, _level), true)
    elseif _Bits.has_bit(_nodemap, idx) then
      if _level == 6 then
        // insert into collision node
        let es = recover _entries.clone() end
        let cs = _entries(i.usize_unsafe())? as _MapCollisions[K, V, H]
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if H.eq(v._1, leaf._1) then
          // update collision
            cs'(k)? = leaf
            es(i.usize_unsafe())? = consume cs'
            return (create(consume es, _nodemap, _datamap, _level), false)
          end
        end
        cs'.push(leaf)
        es(i.usize_unsafe())? = consume cs'
        (create(consume es, _nodemap, _datamap, _level), true)
      else
        // update sub-node
        let sn = _entries(i.usize_unsafe())? as _MapNode[K, V, H]
        let es = recover _entries.clone() end
        (let sn', let u) = sn.update(hash, leaf)? as (_MapNode[K, V, H], Bool)
        es(i.usize_unsafe())? = sn'
        (create(consume es, _nodemap, _datamap, _level), u)
      end
    else
      let old = _entries(i.usize_unsafe())? as _MapLeaf[K, V, H]
      if H.eq(old._1, leaf._1) then
        // update leaf
        let es = recover _entries.clone() end
        es(i.usize())? = leaf
        (create(consume es, _nodemap, _datamap, _level), false)
      elseif _level == 6 then
        // create collision node
        let cn = recover Array[_MapLeaf[K, V, H]](2) end
        cn.push(old)
        cn.push(leaf)
        let es = recover _entries.clone() end
        let dm = _Bits.clear_bit(_datamap, idx)
        let nm = _Bits.set_bit(_nodemap, idx)
        let i' = _compressed_index(nm, dm, idx)
        es.delete(i.usize_unsafe())?
        es.insert(i'.usize_unsafe(), consume cn)?
        (create(consume es, nm, dm, _level), true)
      else
        // create new sub-node
        var sn = empty(_level + 1)
        (sn, _) =
          sn.update(H.hash(old._1).u32(), old)? as (_MapNode[K, V, H], Bool)
        (sn, _) = sn.update(hash, leaf)? as (_MapNode[K, V, H], Bool)
        let es = recover _entries.clone() end
        let nm = _Bits.set_bit(_nodemap, idx)
        let dm = _Bits.clear_bit(_datamap, idx)
        let i' = _compressed_index(nm, dm, idx)
        es.delete(i.usize_unsafe())?
        es.insert(i'.usize_unsafe(), sn)?
        (create(consume es, nm, dm, _level), true)
      end
    end

  fun val remove(hash: U32, key: K): _MapNode[K, V, H] ? =>
    let idx = _Bits.mask(hash, _level)
    let i = _compressed_index(_nodemap, _datamap, idx)
    if i == -1 then error end
    if _Bits.has_bit(_datamap, idx) then
      var es = recover _entries.clone() end
      es.delete(i.usize())?
      create(consume es, _nodemap, _Bits.clear_bit(_datamap, idx), _level)
    else
      if _level == 6 then
        let es = recover _entries.clone() end
        let cs = _entries(i.usize_unsafe())? as _MapCollisions[K, V, H]
        let cs' = recover cs.clone() end
        for (k, v) in cs.pairs() do
          if H.eq(v._1, key) then
            cs'.delete(k)?
            es(i.usize_unsafe())? = consume cs'
            return create(consume es, _nodemap, _datamap, _level)
          end
        end
        error
      else
        var sn = _entries(i.usize_unsafe())? as _MapNode[K, V, H]
        sn = sn.remove(hash, key)?
        let es = recover _entries.clone() end
        if (_nodemap.popcount() == 0) and (_datamap.popcount() == 1) then
          for si in mut.Range[U32](0, 32) do
            if _Bits.has_bit(_datamap, si) then
              es(i.usize_unsafe())? = sn._entries(si.usize_unsafe())?
              return create(consume es, _Bits.clear_bit(_nodemap, idx),
                _Bits.set_bit(_datamap, idx), _level)
            end
          end
        end
        es(i.usize_unsafe())? = sn
        create(consume es, _nodemap, _datamap, _level)
      end
    end

  fun _compressed_index(nm: U32, dm: U32, idx: U32): U32 =>
    let msk = not(0xffff_ffff << idx)
    let np = msk and nm
    let dp = msk and dm
    let i = (np + dp).popcount()
    if _Bits.has_bit(nm, idx) or _Bits.has_bit(dm, idx) then i else -1 end

  fun entries(): Array[_MapEntry[K, V, H]] val => _entries
