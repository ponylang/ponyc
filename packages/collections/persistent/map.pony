interface Hashable
  """
  Anything with a hash method is hashable.
  """
  fun hash(): U64

type Map[K: (Hashable val & Equatable[K] val), V] is
  (LeafNode[K, V] | MultiLeafNode[K, V] | MapNode[K, V])
"""
A persistent map based on Bagwell's hash array mapped trie algorithm.

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

primitive Maps
  fun val empty[K: (Hashable val & Equatable[K] val), V](): Map[K, V] =>
    """
    Return an empty map.
    """
    MapNode[K, V].empty()

  fun val from[K: (Hashable val & Equatable[K] val), V]
  (pairs: Array[(K, val->V)]): Map[K, V] ? =>
    """
    Return a map containing the provided key-value pairs.
    """
    var newMap = empty[K, V]()
    for pair in pairs.values() do
      (let k, let v) = pair
      newMap = newMap.update(k, v)
    end
    newMap

  fun _last_level(): U32 =>
    """
    The maximum depth of the tree used for the map (counting from zero)
    """
    4

class val LeafNode[K: (Hashable val & Equatable[K] val), V]
  let _key: K
  let _value: val->V

  new val create(k: K, v: val->V) =>
    _key = k
    _value = v

  fun size(): U64 =>
    """
    Return the size of the node.
    """
    1

  fun _is_leaf(): Bool =>
    """
    Predicate testing if the node is a leaf.
    """
    true

  fun apply(k: K): val->V ? =>
    """
    Attempt to get the value corresponding to k.
    """
    if k == _key then _value else error end

  fun _get_with_hash(k: K, hash: U32, level: U32): val->V ? =>
    """
    Attempt to get the value corresponding to k, using hash and level.
    """
    apply(k)

  fun update(k: K, v: val->V): Map[K, V] ? =>
    """
    Update the value associated with the provided key.
    """
    if k == _key then
      LeafNode[K, V](k, v) as Map[K, V]
    else
      let mapNode = MapNode[K, V].empty().update(_key, _value)
      mapNode.update(k, v)
    end

  fun _put_with_hash(k: K, v: val->V, hash: U32, level: U32): Map[K, V] ? =>
    """
    Update the value associated with the provided key, using hash and level.
    """
    if k == _key then
      LeafNode[K, V](k, v) as Map[K, V]
    else
      let mapNode = MapNode[K, V].empty()
        ._put_with_hash(_key, _value, MapHelpers._hash[K](_key), level)
      mapNode._put_with_hash(k, v, hash, level)
    end

  fun remove(k: K): Map[K, V] ? =>
    """
    Try to remove the provided key from the node.
    """
    error

  fun _remove_with_hash(k: K, hash: U32, level: U32): Map[K, V] ? =>
    """
    Try to remove the prodied key form the node, using hash and level.
    """
    error

  fun get_or_else(k: K, alt: val->V): val->V =>
    """
    Get the value associated with provided key if present. Otherwise,
    return the provided alternate value.
    """
    try
      apply(k)
    else
      alt
    end

  fun contains(k: K): Bool =>
    """
    Check whether the node contains the provided key.
    """
    try
      apply(k)
      true
    else
      false
    end

class val Entry[K: (Hashable val & Equatable[K] val), V]
  let key: K
  let value: val->V

  new val create(k: K, v: val->V) =>
    key = k
    value = v

class val MultiLeafNode[K: (Hashable val & Equatable[K] val), V]
  let _entries: List[Entry[K, V]]

  new val create() =>
    _entries = Lists[Entry[K, V]].empty()

  new val from(es: List[Entry[K, V]]) =>
    """
    Create a new MultiLeafNode from the provided list of entries.
    """
    _entries = es

  fun size(): U64 =>
    """
    Return the size of the node.
    """
    _entries.size()

  fun _is_leaf(): Bool =>
    """
    Predicate testing if the node is a leaf.
    """
    true

  fun apply(k: K): val->V ? =>
    """
    Attempt to get the value corresponding to k.
    """
    try
      var cur = _entries
      while(cur.is_non_empty()) do
        let next = cur.head()
        if (next.key == k) then return next.value end
        cur = cur.tail()
      end
      error
    else
      error
    end

  fun _get_with_hash(k: K, hash: U32, level: U32): val->V ? =>
    """
    Attempt to get the value corresponding to k, using hash and level.
    """
    apply(k)

  fun update(k: K, v: val->V): Map[K, V] =>
    """
    Update the value associated with the provided key.
    """
    let test =
      object
        let key: K = k
        fun apply(e: Entry[K, V]): Bool => e.key == key
      end

    if _entries.exists(test) then
      _updateEntry(k, v, _entries, Lists[Entry[K, V]].empty())
    else
      let newEntries = _entries.prepend(Entry[K, V](k,v))
      MultiLeafNode[K, V].from(newEntries)
    end

  fun _put_with_hash(k: K, v: val->V, hash: U32, level: U32): Map[K, V] =>
    """
    Update the value associated with the provided key, using hash and level.
    """
    update(k, v)

  fun _updateEntry(k: K,
                   v: val->V,
                   es: List[Entry[K, V]],
                   acc: List[Entry[K, V]]): Map[K, V] =>
    """
    Return a new MultiLeafNode with the entry corresponding to k updated.
    """
    try
      let next = es.head()
      if next.key == k then
        let newEntry = Entry[K, V](k, v)
        let newEntries = acc.prepend(newEntry).concat(es.tail())
        MultiLeafNode[K, V].from(newEntries)
      else
        _updateEntry(k, v, es.tail(), acc.prepend(next))
      end
    else
      MultiLeafNode[K, V].from(acc)
    end

  fun _removeEntry(k: K,
                   es: List[Entry[K, V]],
                   acc: List[Entry[K, V]]): Map[K, V] =>
    """
    Return a new MultiLeafNode with the entry corresponding to k removed.
    """
    try
      let next = es.head()
      if next.key == k then
        let newEntries = acc.concat(es.tail())
        MultiLeafNode[K, V].from(newEntries)
      else
        _removeEntry(k, es.tail(), acc.prepend(next))
      end
    else
      MultiLeafNode[K, V].from(acc)
    end

  fun remove(k: K): Map[K, V] =>
    """
    Try to remove the provided key from the node.
    """
    _removeEntry(k, _entries, Lists[Entry[K, V]].empty())

  fun _remove_with_hash(k: K, hash: U32, level: U32): Map[K, V] =>
    """
    Try to remove the prodied key form the node, using hash and level.
    """
    remove(k)

  fun get_or_else(k: K, alt: val->V): val->V =>
    """
    Get the value associated with provided key if present. Otherwise,
    return the provided alternate value.
    """
    try
      apply(k)
    else
      alt
    end

  fun contains(k: K): Bool =>
    """
    Check whether the node contains the provided key.
    """
    try
      apply(k)
      true
    else
      false
    end

class val MapNode[K: (Hashable val & Equatable[K] val), V]
  let _size: U64
  //Currently, 32-bit bitmap
  let _bitmap: U32
  let _pointers: Array[Map[K, V]] val

  new val create(bmap: U32, ps: Array[Map[K, V]] val) =>
    _bitmap = bmap
    _pointers = ps
    _size = MapHelpers.sum_array_sizes[K, V](_pointers)

  new val empty() =>
    _bitmap = 0
    _pointers = recover val Array[Map[K, V]] end
    _size = 0

  fun size(): U64 =>
    """
    Return the size of the node.
    """
    _size

  fun _is_leaf(): Bool =>
    """
    Predicate testing if the node is a leaf.
    """
    false

  fun apply(k: K): val->V ? =>
    """
    Attempt to get the value corresponding to k.
    """
    let hash = MapHelpers._hash[K](k)
    let level: U32 = 0
    _get_with_hash(k, hash, level)

  fun _get_with_hash(k: K, hash: U32, level: U32): val->V ? =>
    """
    Attempt to get the value corresponding to k, using hash and level.
    """
    let bmapIdx = _BitOps.bitmap_idx_for(hash, level)
    if _BitOps.check_idx_bit(_bitmap, bmapIdx) then
      let arrayIdx = _BitOps.array_idx_for(_bitmap, bmapIdx)
      _pointers(arrayIdx)._get_with_hash(k, hash, level + 1)
    else
      error
    end

  fun update(k: K, v: val->V): Map[K, V] ? =>
    """
    Update the value associated with the provided key.
    """
    let hash = MapHelpers._hash[K](k)
    let level: U32 = 0
    _put_with_hash(k, v, hash, level)

  fun _put_with_hash(k: K, v: val->V, hash: U32, level: U32): Map[K, V] ? =>
    """
    Update the value associated with the provided key, using hash and level.
    """
    if (level >= Maps._last_level()) then
      return _lastLevelput_with_hash(k, v, hash)
    end
    let bmapIdx = _BitOps.bitmap_idx_for(hash, level)
    let arrayIdx: USize = _BitOps.array_idx_for(_bitmap, bmapIdx)
    if _BitOps.check_idx_bit(_bitmap, bmapIdx) then
      let newNode = _pointers(arrayIdx)._put_with_hash(k, v, hash, level + 1)
      let newArray = _overwrite_in_array_at(_pointers, newNode, arrayIdx)
      MapNode[K, V](_bitmap, newArray)
    else
      let newBitMap = _BitOps.flip_indexed_bit_on(_bitmap, bmapIdx)
      let newNode = LeafNode[K, V](k, v)
      let newArray = _insert_in_array_at(_pointers, newNode, arrayIdx)
      MapNode[K, V](newBitMap, newArray)
    end

  fun _lastLevelput_with_hash(k: K, v: val->V, hash: U32): Map[K, V] ? =>
    """
    Update the value associated with the provided key, using the hash on
    the last level.
    """
    let bmapIdx = _BitOps.bitmap_idx_for(hash, Maps._last_level())
    let arrayIdx: USize = _BitOps.array_idx_for(_bitmap, bmapIdx)
    if _BitOps.check_idx_bit(_bitmap, bmapIdx) then
      let newNode = _pointers(arrayIdx).update(k, v)
      let newArray = _overwrite_in_array_at(_pointers, newNode, arrayIdx)
      MapNode[K, V](_bitmap, newArray)
    else
      let newBitMap = _BitOps.flip_indexed_bit_on(_bitmap, bmapIdx)
      let newNode = MultiLeafNode[K, V].update(k,v)
      let newArray = _insert_in_array_at(_pointers, newNode, arrayIdx)
      MapNode[K, V](newBitMap, newArray)
    end

  fun _insert_in_array_at(arr: Array[Map[K, V]] val,
                       node: Map[K, V],
                       idx: USize): Array[Map[K, V]] val ? =>
    """
    Return a new array with the provided node inserted at provided index,
    shifting the old values at and above the index up one.
    """
    var belowArr: USize = 0
    var aboveArr = idx
    let newArray: Array[Map[K, V]] trn = recover trn Array[Map[K, V]] end
    while(belowArr < idx) do
      newArray.push(arr(belowArr))
      belowArr = belowArr + 1
    end
    newArray.push(node)
    while(aboveArr < arr.size()) do
      newArray.push(arr(aboveArr))
      aboveArr = aboveArr + 1
    end
    newArray

  fun _overwrite_in_array_at(arr: Array[Map[K, V]] val,
                          node: Map[K, V],
                          idx: USize): Array[Map[K, V]] val ? =>
    """
    Return a new array with the provided node inserted at provided index.
    """
    var belowArr: USize = 0
    var aboveArr = idx + 1
    let newArray: Array[Map[K, V]] trn = recover trn Array[Map[K, V]] end
    while(belowArr < idx) do
      newArray.push(arr(belowArr))
      belowArr = belowArr + 1
    end
    newArray.push(node)
    while(aboveArr < arr.size()) do
      newArray.push(arr(aboveArr))
      aboveArr = aboveArr + 1
    end
    newArray

  fun _remove_in_array_at(arr: Array[Map[K, V]] val,
                       idx: USize): Array[Map[K, V]] val ? =>
    """
    Return a new array with the value at the provided index removed and values
    above the index shifted down one.
    """
    var belowArr: USize = 0
    var aboveArr = idx + 1
    let newArray: Array[Map[K, V]] trn = recover trn Array[Map[K, V]] end
    while(belowArr < idx) do
      newArray.push(arr(belowArr))
      belowArr = belowArr + 1
    end
    while(aboveArr < arr.size()) do
      newArray.push(arr(aboveArr))
      aboveArr = aboveArr + 1
    end
    newArray

  fun remove(k: K): Map[K, V] ? =>
    """
    Try to remove the provided key from the node.
    """
    if contains(k) then
      _remove_with_hash(k, MapHelpers._hash[K](k), 0)
    else
      this as Map[K, V]
    end

  fun _remove_with_hash(k: K, hash: U32, level: U32): Map[K, V] ? =>
    """
    Try to remove the prodied key form the node, using hash and level.
    """
    let bmapIdx = _BitOps.bitmap_idx_for(hash, level)
    let arrayIdx: USize = _BitOps.array_idx_for(_bitmap, bmapIdx)
    if level >= Maps._last_level() then
      let newNode = _pointers(arrayIdx).remove(k)
      MapNode[K, V](_bitmap, _overwrite_in_array_at(_pointers, newNode, arrayIdx))
    else
      let target = _pointers(arrayIdx)
      if target._is_leaf() then
        let newBMap = _BitOps.flip_indexed_bit_off(_bitmap, bmapIdx)
        let newArray = _remove_in_array_at(_pointers, arrayIdx)
        MapNode[K, V](newBMap, newArray)
      else
        let newNode = target._remove_with_hash(k, hash, level + 1)
        let newArray = _overwrite_in_array_at(_pointers, newNode, arrayIdx)
        MapNode[K, V](_bitmap, newArray)
      end
    end

  fun get_or_else(k: K, alt: val->V): val->V =>
    """
    Get the value associated with provided key if present. Otherwise,
    return the provided alternate value.
    """
    try
      apply(k)
    else
      alt
    end

  fun contains(k: K): Bool =>
    """
    Check whether the node contains the provided key.
    """
    try
      apply(k)
      true
    else
      false
    end

// For 32-bit operations
primitive _BitOps
  fun mask_low(n: U32): U32 =>
    """
    Mask the low five bits
    """
    n and 0x1F

  fun bitmap_idx_for(hash: U32, level: U32): U32 =>
    """
    Calculate the bitmap index from the next five bits of the hash
    """
    mask_low(hash >> (level * 5))

  fun check_idx_bit(bmap: U32, idx: U32): Bool =>
    """
    Check the bit corresponding to the provided index
    """
    let bit = (bmap >> idx) and 1
    if bit == 0 then false else true end

  fun array_idx_for(bmap: U32, idx: U32): USize =>
    """
    Check the array index for the map node corresponding to
    the provided bitmap index. Since the pointers array only includes
    map nodes that are present, we use the popcount of the bits
    below the bitmap index.
    """
    let mask = not(0xFFFF_FFFF << idx)
    ((mask and bmap).popcount()).usize()

  fun flip_indexed_bit_on(bmap: U32, idx: U32): U32 =>
    """
    Return a new bitmap with the index bit set to 1.
    """
    (1 << idx) or bmap

  fun flip_indexed_bit_off(bmap: U32, idx: U32): U32 =>
    """
    Return a new bitmap with the index bit set to 0.
    """
    not(1 << idx) and bmap

primitive MapHelpers
  fun sum_array_sizes[K: (Hashable val & Equatable[K] val), V]
  (arr: Array[Map[K, V]] val): U64 =>
    """
    Sum the sizes of all maps in the provided array.
    """
    var sum: U64 = 0
    for m in arr.values() do
      sum = sum + m.size()
    end
    sum

  fun _hash[K: Hashable val](k: K): U32 =>
    """
    Return a 32-bit hash of the key.
    """
    k.hash().u32()
