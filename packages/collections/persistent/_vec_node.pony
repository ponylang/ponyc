type _VecSubNodes[A: Any #share] is Array[_VecNode[A]]

class val _VecNode[A: Any #share]
  let _entries: (Array[A] val | _VecSubNodes[A] val)

  new val empty(depth: USize) =>
    _entries =
      recover
        if depth == 0
        then Array[A]
        else _VecSubNodes[A]
        end
      end

  new val create(entries': (Array[A] val | _VecSubNodes[A] val)) =>
    _entries = entries'

  fun val grow_root(): _VecNode[A] =>
    create(recover Array[_VecNode[A]](1) .> push(this) end)

  fun apply(depth: USize, i: USize): A ? =>
    let idx = _Bits.mask(i, depth)
    match \exhaustive\ _entries
    | let ls: Array[A] box => ls(idx)?
    | let ns: _VecSubNodes[A] box => ns(idx)?(depth - 1, i)?
    end

  fun val push(depth: USize, i: USize, tail: Array[A] val): _VecNode[A] ? =>
    let ns = _entries as _VecSubNodes[A] val
    if depth == 1 then
      let ls = create(tail)
      create(recover ns.clone() .> push(ls) end)
    else
      let idx = _Bits.mask(i, depth)
      if _entries.size() > idx then
        let sn = ns(idx)?.push(depth - 1, i, tail)?
        create(recover ns.clone() .> update(idx, sn)? end)
      else
        let sn = empty(depth - 1).push(depth - 1, i, tail)?
        create(recover ns.clone() .> push(sn) end)
      end
    end

  fun val pop(depth: USize, i: USize): (_VecNode[A], Array[A] val) ? =>
    match \exhaustive\ _entries
    | let ls: Array[A] val => (this, ls)
    | let ns: _VecSubNodes[A] val =>
      let idx = _Bits.mask(i, depth)
      if depth == 1 then
        let tail = ns(idx)?.pop(depth - 1, i)?._2
        (create(ns.trim(0, ns.size() - 1)), tail)
      else
        (let sn, let tail) = ns(idx)?.pop(depth - 1, i)?
        (create(recover ns.clone() .> update(idx, sn)? end), tail)
      end
    end

  fun val update(depth: USize, i: USize, v: A): _VecNode[A] ? =>
    let idx = _Bits.mask(i, depth)
    match \exhaustive\ _entries
    | let ls: Array[A] val =>
      create(recover ls.clone() .> update(idx, v)? end)
    | let ns: _VecSubNodes[A] val =>
      let sn = ns(idx)?.update(depth - 1, i, v)?
      create(recover ns.clone() .> update(idx, sn)? end)
    end

  fun val leaf_nodes(lns: Array[Array[A] val]): Array[Array[A] val]^ =>
    match \exhaustive\ _entries
    | let lns': Array[A] val =>
      lns.push(lns')
    | let ns: _VecSubNodes[A] val =>
      for sn in ns.values() do sn.leaf_nodes(lns) end
    end
    lns
