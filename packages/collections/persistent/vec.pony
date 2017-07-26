use mut = "collections"

class val Vec[A: Any #share]
  """
  A persistent vector based on the Hash Array Mapped Trie from 'Ideal Hash
  Trees' by Phil Bagwell.
  """

  let _root: (_VecNode[A] | None)
  let _tail: Array[A] val
  let _size: USize
  let _depth: U8
  let _tail_offset: USize

  new val create() =>
    _root = None
    _tail = recover Array[A] end
    _size = 0
    _depth = 1
    _tail_offset = 0

  new val _create(
    root': (_VecNode[A] | None),
    tail': Array[A] val,
    size': USize,
    depth': U8,
    tail_offset': USize)
  =>
    _root = root'
    _tail = tail'
    _size = size'
    _depth = depth'
    _tail_offset = tail_offset'

  fun size(): USize =>
    """
    Return the amount of values in the vector.
    """
    _size

  fun apply(i: USize): val->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    if i < _tail_offset then
      (_root as _VecNode[A])(i)?
    else
      _tail(i - _tail_offset)?
    end

  fun val update(i: USize, value: val->A): Vec[A] ? =>
    """
    Return a vector with the i-th element changed, raising an error if the
    index is out of bounds.
    """
    if i < _tail_offset then
      let root = (_root as _VecNode[A]).update(i, value)?
      _create(root, _tail, _size, _depth, _tail_offset)
    else
      let tail =
        recover val _tail.clone() .> update(i - _tail_offset, value)? end
      _create(_root, tail, _size, _depth, _tail_offset)
    end

  fun val insert(i: USize, value: val->A): Vec[A] ? =>
    """
    Return a vector with an element inserted. Elements after this are moved
    up by one index, extending the vector. An out of bounds index raises an
    error.
    """
    if i >= _size then error end
    var vec = this
    var prev = value
    for idx in mut.Range(i, _size) do
      vec = vec.update(idx, prev = this(idx)?)?
    end
    vec.push(this(_size - 1)?)

  fun val delete(i: USize): Vec[A] ? =>
    """
    Return a vector with an element deleted. Elements after this are moved
    down by one index, compacting the vector. An out of bounds index raises an
    error.
    """
    if i >= _size then error end
    var vec = pop()?
    for idx in mut.Range(i + 1, _size) do
      vec = vec.update(idx - 1, this(idx)?)?
    end
    vec

  fun val remove(i: USize, n: USize): Vec[A] ? =>
    """
    Return a vector with n elements removed, beginning at index i.
    """
    if i >= _size then error end
    var vec = this
    for _ in mut.Range(0, n) do vec = vec.pop()? end
    for idx in mut.Range(i, _size - n) do
      vec = vec.update(idx, this(idx + n)?)?
    end
    vec

  fun val push(value: val->A): Vec[A] =>
    """
    Return a vector with the value added to the end.
    """
    if _tail.size() < 32 then
      // push value into tail
      let tail = recover val _tail.clone() .> push(value) end
      _create(_root, tail, _size + 1, _depth, _tail_offset)
    elseif _tail_offset == _pow32(_depth.usize_unsafe()) then
      // create new root
      // push tail into root
      // push value into new tail
      let root =
        try (_root as _VecNode[A]).new_root().push(_tail_offset, _tail)?
        else _root
        end
      let tail = recover val Array[A](1) .> push(value) end
      _create(root, tail, _size + 1, _depth + 1, _tail_offset + 32)
    else
      // push tail into root
      // push value into new tail
      let root =
        match _root
        | let r: _VecNode[A] => try r.push(_tail_offset, _tail)? else r end
        | None =>
          let r = _VecNode[A].empty(1)
          try r.push(0, _tail)? else r end
        end
      let tail = recover val Array[A](1) .> push(value) end
      _create(root, tail, _size + 1, _depth, _tail_offset + 32)
    end

  fun val pop(): Vec[A] ? =>
    """
    Return a vector with the value at the end removed.
    """
    if (_tail.size() > 0) or (_size == 1) then
      let tail = recover val _tail.clone() .> pop()? end
      _create(_root, tail, _size - 1, _depth, _tail_offset)
    else
      let tail_offset = _tail_offset - 32
      (let root, let tail) = (_root as _VecNode[A]).pop(tail_offset)?
      _create(root, tail, _size - 1, _depth, tail_offset)
    end

  fun val concat(iter: Iterator[val->A]): Vec[A] =>
    """
    Return a vector with the values of the given iterator added to the end.
    """
    var v = this
    for a in iter do
      v = v.push(a)
    end
    v

  fun val find(
    value: val->A,
    offset: USize = 0,
    nth: USize = 0,
    predicate: {(A, A): Bool} val = {(l: A, r: A): Bool => l is r })
    : USize ?
  =>
    """
    Find the `nth` appearance of `value` from the beginning of the vector,
    starting at `offset` and examining higher indices, and using the
    supplied `predicate` for comparisons. Returns the index of the value, or
    raise an error if the value isn't present.

    By default, the search starts at the first element of the vector,
    returns the first instance of `value` found, and uses object identity
    for comparison.
    """
    var n: USize = 0
    for i in mut.Range(offset, _size) do
      if predicate(this(i)?, value) then
        if n == nth then return i end
        n = n + 1
      end
    end
    error

  fun val contains(
    value: val->A,
    predicate: {(A, A): Bool} val = {(l: A, r: A): Bool => l is r })
    : Bool
  =>
    """
    Returns true if the vector contains `value`, false otherwise.
    """
    for v in values() do
      if predicate(v, value) then return true end
    end
    false

  fun val slice(from: USize = 0, to: USize = -1, step: USize = 1): Vec[A] =>
    """
    Return a vector that is a clone of a portion of this vector. The range is
    exclusive and saturated.
    """
    var vec = Vec[A]
    for i in mut.Range(0, if _size < to then _size else to end, step) do
      try vec.push(this(i)?) end
    end
    vec

  fun val reverse(): Vec[A] =>
    """
    Return a vector with the elements in reverse order.
    """
    var vec = Vec[A]
    for i in mut.Reverse(_size - 1, 0) do
      try vec = vec.push(this(i)?) end
    end
    vec

  fun val keys(): VecKeys[A]^ =>
    """
    Return an iterator over the indices in the vector.
    """
    VecKeys[A](this)

  fun val values(): VecValues[A]^ =>
    """
    Return an iterator over the values in the vector.
    """
    VecValues[A](this)

  fun val pairs(): VecPairs[A]^ =>
    """
    Return an iterator over the (index, value) pairs in the vector.
    """
    VecPairs[A](this)

  fun _pow32(n: USize): USize =>
    """
    Raise 32 to the power of n.
    """
    if n == 0 then
      1
    else
      32 << ((n - 1) * 5)
    end

  fun _leaf_nodes(): Array[Array[A] val]^ =>
    let lns = Array[Array[A] val](_size / 32)
    match _root
    | let vn: _VecNode[A] => vn.leaf_nodes(lns)
    end
    if _tail.size() > 0 then lns.push(_tail) end
    lns

class VecKeys[A: Any #share]
  embed _pairs: VecPairs[A]

  new create(v: Vec[A]) => _pairs = VecPairs[A](v)

  fun has_next(): Bool => _pairs.has_next()

  fun ref next(): USize ? => _pairs.next()?._1

class VecValues[A: Any #share]
  embed _pairs: VecPairs[A]

  new create(v: Vec[A]) => _pairs = VecPairs[A](v)

  fun has_next(): Bool => _pairs.has_next()

  fun ref next(): val->A ? => _pairs.next()?._2

class VecPairs[A: Any #share]
  let _leaf_nodes: Array[Array[A] val]
  var _idx: USize = 0
  var _i: USize = 0

  new create(v: Vec[A]) =>
    _leaf_nodes = v._leaf_nodes()

  fun has_next(): Bool =>
    _leaf_nodes.size() > 0

  fun ref next(): (USize, A) ? =>
    var leaves = _leaf_nodes(0)?
    let v = leaves(_idx = _idx + 1)?

    if _idx == leaves.size() then
      _leaf_nodes.shift()?
      _idx = 0
    end
    (_i = _i + 1, v)
