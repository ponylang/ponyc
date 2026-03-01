use mut = "collections"

class val _MapEntry[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  let key: K
  let value: V

  new val create(k: K, v: V) =>
    key = k
    value = v

  fun apply(k: K): V ? =>
    if H.eq(k, key) then value else error end

class val _MapCollisions[
  K: Any #share, V: Any #share, H: mut.HashFunction[K] val]

  embed bins: Array[Array[_MapEntry[K, V, H]] iso] iso

  new iso create() =>
    bins = recover Array[Array[_MapEntry[K, V, H]] iso](4) end
    bins .> push([]) .> push([]) .> push([]) .> push([])

  fun val clone(): _MapCollisions[K, V, H] iso^ =>
    let cs = _MapCollisions[K, V, H]
    try
      for i in bins.keys() do
        for e in bins(i)?.values() do
          cs.bins(i)?.push(e)
        end
      end
    end
    cs

  fun val apply(hash: U32, k: K): V ? =>
    let idx = _Bits.mask32(hash, _Bits.collision_depth())
    let bin = bins(idx.usize_unsafe())?
    for node in bin.values() do
      if H.eq(k, node.key) then return node.value end
    end
    error

  fun val remove(hash: U32, k: K): _MapCollisions[K, V, H] ? =>
    let idx = _Bits.mask32(hash, _Bits.collision_depth())
    let bin = bins(idx.usize_unsafe())?
    for (i, node) in bin.pairs() do
      if H.eq(k, node.key) then
        let bin' = recover bin.clone() end
        bin'.delete(i)?
        let n = clone()
        n.bins(idx.usize_unsafe())? = consume bin'
        return consume n
      end
    end
    error

  fun val pull_last(): _MapEntry[K, V, H] ? =>
    for bin in bins.values() do
      if bin.size() > 0 then return bin(0)? end
    end
    error

  fun ref update_mut(hash: U32, entry: _MapEntry[K, V, H]): Bool ? =>
    let idx = _Bits.mask32(hash, _Bits.collision_depth())
    for i in mut.Range(0, bins(idx.usize_unsafe())?.size()) do
      let e = bins(idx.usize_unsafe())?(i)?
      if H.eq(entry.key, e.key) then
        bins(idx.usize_unsafe())?(i)? = entry
        return false
      end
    end
    bins(idx.usize_unsafe())?.push(entry)
    true

  fun val iter(): _MapIter[K, V, H] =>
    let stack = Array[Iterator[_MapEntry[K, V, H]]]
    for bin in bins.values() do
      if bin.size() > 0 then
        stack.push(bin.values())
      end
    end

    object ref
      fun ref has_next(): Bool =>
        stack.size() > 0

      fun ref next(): _MapEntry[K, V, H] ? =>
        let iter = stack(0)?
        let e = iter.next()?
        if not iter.has_next() then stack.shift()? end
        e
    end

type _MapNode[K: Any #share, V: Any #share, H: mut.HashFunction[K] val] is
  ( _MapEntry[K, V, H]
  | _MapCollisions[K, V, H]
  | _MapSubNodes[K, V, H]
  )

class val _MapSubNodes[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  embed nodes: Array[_MapNode[K, V, H]] iso
  var node_map: U32
  var data_map: U32

  new iso create(size: USize = 0, nm: U32 = 0, dm: U32 = 0) =>
    nodes = recover Array[_MapNode[K, V, H]](size) end
    node_map = nm
    data_map = dm

  fun val clone(): _MapSubNodes[K, V, H] iso^ =>
    let ns = _MapSubNodes[K, V, H](nodes.size(), node_map, data_map)
    for node in nodes.values() do ns.nodes.push(node) end
    ns

  fun compressed_idx(idx: U32): U32 =>
    if not _Bits.check_bit(node_map or data_map, idx) then return -1 end
    let msk = not (U32(-1) << idx)
    if _Bits.check_bit(data_map, idx) then
      return (data_map and msk).popcount()
    end
    data_map.popcount() + (node_map and msk).popcount()

  fun val apply(depth: U32, hash: U32, k: K): V ? =>
    let idx = _Bits.mask32(hash, depth)
    let c_idx = compressed_idx(idx)
    if c_idx == -1 then error end
    match \exhaustive\ nodes(c_idx.usize_unsafe())?
    | let entry: _MapEntry[K, V, H] => entry(k)?
    | let sns: _MapSubNodes[K, V, H] => sns(depth + 1, hash, k)?
    | let cs: _MapCollisions[K, V, H] => cs(hash, k)?
    end

  fun val update(depth: U32, hash: U32, k: K, v: V)
    : (_MapSubNodes[K, V, H], Bool) ?
  =>
    let node = clone()
    let r = node.update_mut(depth, hash, k, v)?
    (consume node, r)

  fun ref update_mut(depth: U32, hash: U32, k: K, v: V): Bool ? =>
    let idx = _Bits.mask32(hash, depth)
    var c_idx = compressed_idx(idx)

    if c_idx == -1 then
      data_map = _Bits.set_bit(data_map, idx)
      c_idx = compressed_idx(idx)
      nodes.insert(c_idx.usize_unsafe(), _MapEntry[K, V, H](k, v))?
      return true
    end

    if _Bits.check_bit(node_map, idx) then
      var insert = false
      let c_idx_u = c_idx.usize_unsafe()
      if depth < (_Bits.collision_depth() - 1) then
        let sn = (nodes(c_idx_u)? as _MapSubNodes[K, V, H]).clone()
        insert = sn.update_mut(depth + 1, hash, k, v)?
        nodes(c_idx_u)? = consume sn
      else
        let cs = (nodes(c_idx_u)? as _MapCollisions[K, V, H]).clone()
        insert = cs.update_mut(hash, _MapEntry[K, V, H](k, v))?
        nodes(c_idx_u)? = consume cs
      end
      return insert
    end

    // if not _Bits.check_bit(data_map, idx) then error end

    let entry0 = nodes(c_idx.usize_unsafe())? as _MapEntry[K, V, H]
    if H.eq(k, entry0.key) then
      nodes(c_idx.usize_unsafe())? = _MapEntry[K, V, H](k, v)
      return false
    end

    let sub_node: _MapNode[K, V, H] val =
      if depth < (_Bits.collision_depth() - 1) then
        let hash0 = H.hash(entry0.key).u32()
        let idx0 = _Bits.mask32(hash0, depth + 1)
        let sn = _MapSubNodes[K, V, H](1, 0, _Bits.set_bit(0, idx0))
        sn.nodes.push(entry0)
        sn.update_mut(depth + 1, hash, k, v)?
        consume sn
      else
        let hash0 = H.hash(entry0.key).u32()
        let idx0 = _Bits.mask32(hash0, _Bits.collision_depth())
        let idx1 = _Bits.mask32(hash, _Bits.collision_depth())
        let sn = _MapCollisions[K, V, H]
        sn.bins(idx0.usize_unsafe())?.push(entry0)
        sn.bins(idx1.usize_unsafe())?.push(_MapEntry[K, V, H](k, v))
        consume sn
      end

    nodes.delete(c_idx.usize_unsafe())?
    data_map = _Bits.clear_bit(data_map, idx)
    node_map = _Bits.set_bit(node_map, idx)
    c_idx = compressed_idx(idx)
    nodes.insert(c_idx.usize_unsafe(), sub_node)?
    true

  fun val remove(depth: U32, hash: U32, k: K): _MapSubNodes[K, V, H] ? =>
    let idx = _Bits.mask32(hash, depth)
    var c_idx = compressed_idx(idx)

    if c_idx == -1 then error end

    let ns = clone()
    if _Bits.check_bit(data_map, idx) then
      ns.data_map = _Bits.clear_bit(data_map, idx)
      ns.nodes.delete(c_idx.usize_unsafe())?
    else
      match \exhaustive\ nodes(c_idx.usize_unsafe())?
      | let entry: _MapEntry[K, V, H] val => error
      | let sns: _MapSubNodes[K, V, H] val =>
        let sn = sns.remove(depth + 1, hash, k)?
        if (sn.nodes.size() == 1) and (sn.data_map != 0) then
          // compact
          ns.node_map = _Bits.clear_bit(ns.node_map, idx)
          ns.data_map = _Bits.set_bit(ns.data_map, idx)
          ns.nodes.delete(c_idx.usize_unsafe())?
          c_idx = ns.compressed_idx(idx)
          ns.nodes.insert(c_idx.usize_unsafe(), sn.nodes(0)?)?
        else
          ns.nodes(c_idx.usize_unsafe())? = sn
        end
      | let cs: _MapCollisions[K, V, H] val =>
        let sn = cs.remove(hash, k)?
        var n: USize = 0
        for bin in sn.bins.values() do n = n + bin.size() end
        if n == 1 then
          // compact
          ns.node_map = _Bits.clear_bit(ns.node_map, idx)
          ns.data_map = _Bits.set_bit(ns.data_map, idx)
          ns.nodes.delete(c_idx.usize_unsafe())?
          c_idx = ns.compressed_idx(idx)
          ns.nodes.insert(c_idx.usize_unsafe(), sn.pull_last()?)?
        else
          ns.nodes(c_idx.usize_unsafe())? = cs.remove(hash, k)?
        end
      end
    end
    ns

  fun val iter(): _MapIter[K, V, H] =>
    object ref
      let node: _MapSubNodes[K, V, H] = this
      var _idx: USize = 0

      fun ref has_next(): Bool =>
        _idx < node.nodes.size()

      fun ref next(): (_MapEntry[K, V, H] | _MapIter[K, V, H]) ? =>
        match \exhaustive\ node.nodes(_idx = _idx + 1)?
        | let e: _MapEntry[K, V, H] => e
        | let ns: _MapSubNodes[K, V, H] => ns.iter()
        | let cs: _MapCollisions[K, V, H] => cs.iter()
        end
    end
