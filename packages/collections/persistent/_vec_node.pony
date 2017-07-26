type _VecLeafNodes[A: Any #share] is Array[Array[A] val] val
type _VecNodes[A: Any #share] is Array[_VecNode[A]] val

type _VecEntries[A: Any #share] is (_VecNodes[A] | _VecLeafNodes[A])

class val _VecNode[A: Any #share]
  let _entries: _VecEntries[A]
  let _level: U8

  new val empty(level: U8) =>
    _entries =
      if level == 1 then recover Array[Array[A] val] end
      else recover Array[_VecNode[A]] end
      end
    _level = level

  new val create(entries: _VecEntries[A], level: U8) =>
    (_entries, _level) = (consume entries, level)

  fun val new_root(): _VecNode[A] =>
    let entries = recover val Array[_VecNode[A]](1) .> push(this) end
    create(entries, _level + 1)

  fun apply(i: USize): A ? =>
    let idx = _Bits.mask(i.u32_unsafe(), _level).usize_unsafe()
    match _entries
    | let sns: _VecNodes[A] => sns(idx)?(i)?
    | let lns: _VecLeafNodes[A] =>
      lns(idx)?(_Bits.mask(i.u32_unsafe(), _level - 1).usize_unsafe())?
    end

  fun val update(i: USize, value: A): _VecNode[A] ? =>
    let idx = _Bits.mask(i.u32_unsafe(), _level).usize_unsafe()
    match _entries
    | let sns: _VecNodes[A] =>
      let sn = sns(idx)?.update(i, value)?
      let entries = recover val sns.clone() .> update(idx, sn)? end
      create(entries, _level)
    | let lns: _VecLeafNodes[A] =>
      let li = _Bits.mask(i.u32_unsafe(), _level - 1).usize_unsafe()
      let ln = recover val lns(idx)?.clone() .> update(li, value)? end
      let entries = recover val lns.clone() .> update(idx, ln)? end
      create(entries, _level)
    end

  fun val push(i: USize, tail: Array[A] val): _VecNode[A] ? =>
    // this method must never be called before a node resize (if required)
    match _entries
    | let sns: _VecNodes[A] =>
      let idx = _Bits.mask(i.u32_unsafe(), _level).usize_unsafe()
      if _entries.size() > idx then
        let sn = (_entries as _VecNodes[A])(idx)?.push(i, tail)?
        let entries = recover val sns.clone() .> update(idx, sn)? end
        create(entries, _level)
      else
        let sn = empty(_level - 1).push(i, tail)?
        let entries = recover val sns.clone() .> push(sn) end
        create(entries, _level)
      end
    | let lns: _VecLeafNodes[A] =>
      let entries = recover val lns.clone() .> push(tail) end
      create(entries, _level)
    end

  fun val pop(i: USize): (_VecNode[A], Array[A] val) ? =>
    let idx = _Bits.mask(i.u32_unsafe(), _level).usize_unsafe()
    match _entries
    | let sns: _VecNodes[A] =>
      (let sn', let tail) = sns(idx)?.pop(i)?
      let entries = recover val sns.clone() .> update(idx, sn')? end
      (create(entries, _level), tail)
    | let lns: _VecLeafNodes[A] =>
      let ln = recover val lns(idx)?.clone() .> pop()? end
      let lns' = recover val lns.clone() .> pop()? end
      (create(lns', _level), ln)
    end

  fun val leaf_nodes(lns: Array[Array[A] val]): Array[Array[A] val]^ =>
    match _entries
    | let sns: _VecNodes[A] =>
      for sn in sns.values() do sn.leaf_nodes(lns) end
    | let lns': _VecLeafNodes[A] =>
      lns.append(lns')
    end
    lns
