use mut = "collections"

class val _MapEntry[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  let key: K
  let value: V

  new val create(k: K, v: V) =>
    key = k
    value = v

  fun apply(k: K): (V | None) =>
    if H.eq(k, key) then value end

  // fun _debug(): String iso^ =>
  //   recover
  //     String
  //       .> append("(")
  //       .> append(iftype K <: Stringable val then key.string() else "?" end)
  //       .> append(", ")
  //       .> append(iftype V <: Stringable val then value.string() else "?" end)
  //       .> append(")")
  //   end

class val _MapCollisions[
  K: Any #share, V: Any #share, H: mut.HashFunction[K] val]

  embed bins: Array[Array[_MapEntry[K, V, H]] trn] trn

  new trn create() =>
    bins = recover Array[Array[_MapEntry[K, V, H]] trn](4) end
    bins .> push([]) .> push([]) .> push([]) .> push([])

  fun val clone(): _MapCollisions[K, V, H] trn^ =>
    let cs = recover trn _MapCollisions[K, V, H] end
    try
      for i in bins.keys() do
        for e in bins(i)?.values() do
          cs.bins(i)?.push(e)
        end
      end
    end
    consume cs

  fun apply(hash: U32, k: K): (V | None) ? =>
    let idx = _Bits.mask32(hash, _Bits.collision_depth())
    let bin = bins(idx.usize_unsafe())?
    for node in bin.values() do
      if H.eq(k, node.key) then return node.value end
    end

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

  fun ref put_mut(hash: U32, entry: _MapEntry[K, V, H]): Bool ? =>
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

    object ref is _MapIter[K, V, H]
      fun ref has_next(): Bool =>
        stack.size() > 0

      fun ref next(): _MapEntry[K, V, H] ? =>
        let iter = stack(0)?
        let e = iter.next()?
        if not iter.has_next() then stack.shift()? end
        e
    end

  // fun _debug(): String iso^ =>
  //   let strs = [as Stringable: "<"]
  //   for bin in bins.values() do strs.push("[" + bin.size().string() + "]") end
  //   " ".join(strs .> push(">") .values())

type _MapNode[K: Any #share, V: Any #share, H: mut.HashFunction[K] val] is
  ( _MapEntry[K, V, H]
  | _MapCollisions[K, V, H]
  | _MapSubNodes[K, V, H]
  )

class val _MapSubNodes[K: Any #share, V: Any #share, H: mut.HashFunction[K] val]
  embed nodes: Array[_MapNode[K, V, H]] trn
  var node_map: U32
  var data_map: U32

  new trn create(size: USize = 0, nm: U32 = 0, dm: U32 = 0) =>
    nodes = recover Array[_MapNode[K, V, H]](size) end
    node_map = nm
    data_map = dm

  fun val clone(): _MapSubNodes[K, V, H] trn^ =>
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

  fun apply(depth: U32, hash: U32, k: K): (V | None) ? =>
    let idx = _Bits.mask32(hash, depth)
    let c_idx = compressed_idx(idx)
    if c_idx == -1 then return None end
    match nodes(c_idx.usize_unsafe())?
    | let entry: _MapEntry[K, V, H] box => entry(k)
    | let sns: _MapSubNodes[K, V, H] box => sns(depth + 1, hash, k)?
    | let cs: _MapCollisions[K, V, H] box => cs(hash, k)?
    end

  // TODO: compact on remove
  fun val remove(depth: U32, hash: U32, k: K): _MapSubNodes[K, V, H] ? =>
    let idx = _Bits.mask32(hash, depth)
    let c_idx = compressed_idx(idx)

    if c_idx == -1 then error end

    let ns = clone()
    if _Bits.check_bit(data_map, idx) then
      ns.data_map = _Bits.clear_bit(data_map, idx)
      ns.nodes.delete(c_idx.usize_unsafe())?
    else
      match nodes(c_idx.usize_unsafe())?
      | let entry: _MapEntry[K, V, H] val => error
      | let sns: _MapSubNodes[K, V, H] val =>
        let sn = sns.remove(depth + 1, hash, k)?
        if (sn.nodes.size() == 1) and (sn.data_map != 0) then
          // compact
          ns.node_map = _Bits.clear_bit(node_map, idx)
          ns.data_map = _Bits.set_bit(data_map, idx)
          ns.nodes.delete(c_idx.usize_unsafe())?
          let c_idx' = ns.compressed_idx(idx)
          ns.nodes.insert(c_idx'.usize_unsafe(), sn.nodes(0)?)?
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
          let c_idx' = ns.compressed_idx(idx)
          sn.pull_last()?
          ns.nodes.insert(c_idx'.usize_unsafe(), sn.pull_last()?)?
        else
          ns.nodes(c_idx.usize_unsafe())? = cs.remove(hash, k)?
        end
      end
    end
    ns

  fun ref put_mut(depth: U32, hash: U32, k: K, v: V): Bool ? =>
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
      if depth < (_Bits.collision_depth() - 1) then
        let sn =
          (nodes(c_idx.usize_unsafe())? as _MapSubNodes[K, V, H]).clone()
        insert = sn.put_mut(depth + 1, hash, k, v)?
        nodes(c_idx.usize_unsafe())? = consume sn
      else
        let cs =
          (nodes(c_idx.usize_unsafe())? as _MapCollisions[K, V, H]).clone()
        insert = cs.put_mut(hash, _MapEntry[K, V, H](k, v))?
        nodes(c_idx.usize_unsafe())? = consume cs
      end
      return insert
    end

    // if not _Bits.check_bit(data_map, idx) then error end

    let entry0 = nodes(c_idx.usize_unsafe())? as _MapEntry[K, V, H]
    if H.eq(k, entry0.key) then
      nodes(c_idx.usize_unsafe())? = _MapEntry[K, V, H](k, v)
      return false
    end

    if depth < (_Bits.collision_depth() - 1) then
      let hash0 = H.hash(entry0.key).u32()
      let idx0 = _Bits.mask32(hash0, depth + 1)
      let sub_node = _MapSubNodes[K, V, H](1, 0, _Bits.set_bit(0, idx0))
      sub_node.nodes.push(entry0)
      sub_node.put_mut(depth + 1, hash, k, v)?

      nodes.delete(c_idx.usize_unsafe())?
      data_map = _Bits.clear_bit(data_map, idx)
      node_map = _Bits.set_bit(node_map, idx)
      c_idx = compressed_idx(idx)
      nodes.insert(c_idx.usize_unsafe(), consume sub_node)?
    else
      let sub_node = _MapCollisions[K, V, H]
      let hash0 = H.hash(entry0.key).u32()
      let idx0 = _Bits.mask32(hash0, _Bits.collision_depth())
      sub_node.bins(idx0.usize_unsafe())?.push(entry0)
      let idx1 = _Bits.mask32(hash, _Bits.collision_depth())
      sub_node.bins(idx1.usize_unsafe())?.push(_MapEntry[K, V, H](k, v))

      nodes.delete(c_idx.usize_unsafe())?
      data_map = _Bits.clear_bit(data_map, idx)
      node_map = _Bits.set_bit(node_map, idx)
      c_idx = compressed_idx(idx)
      nodes.insert(c_idx.usize_unsafe(), consume sub_node)?
    end
    true

  fun val put(depth: U32, hash: U32, k: K, v: V)
    : (_MapSubNodes[K, V, H], Bool) ?
  =>
    let node = clone()
    let r = node.put_mut(depth, hash, k, v)?
    (consume node, r)

  fun val iter(): _MapIter[K, V, H] =>
    object ref is _MapIter[K, V, H]
      let node: _MapSubNodes[K, V, H] = this
      var _idx: USize = 0

      fun ref has_next(): Bool =>
        _idx < node.nodes.size()

      fun ref next(): (_MapEntry[K, V, H] | _MapIter[K, V, H]) ? =>
        match node.nodes(_idx = _idx + 1)?
        | let e: _MapEntry[K, V, H] => e
        | let ns: _MapSubNodes[K, V, H] => ns.iter()
        | let cs: _MapCollisions[K, V, H] => cs.iter()
        end
    end

  // fun val _debug_path(depth: U32, hash: U32) =>
  //   let idx = _Bits.mask32(hash, depth)
  //   let c_idx = compressed_idx(idx).usize()
  //   let strs = [as Stringable: "{"]
  //   for (i, node) in nodes.pairs() do
  //     strs.push(
  //       match node
  //       | let e: _MapEntry[K, V, H] => e._debug()
  //       | let ns: _MapSubNodes[K, V, H] =>
  //         if i == c_idx then "**" else "{}" end
  //       | let cs: _MapCollisions[K, V, H] =>
  //         if i == c_idx then cs._debug() else "<>" end
  //       end)
  //   end
  //   Debug(strs .> push("}"), " ")
  //   try
  //     match nodes(c_idx)?
  //     | let ns: _MapSubNodes[K, V, H] => ns._debug_path(depth+1, hash)
  //     end
  //   end
