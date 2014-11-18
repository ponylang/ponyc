interface Hashable
  fun box hash(): U64

primitive _MapEmpty is Comparable[_MapEmpty]
primitive _MapDeleted is Comparable[_MapDeleted]

class Map[Key: (Hashable box & Comparable[Key] box), Value]
  var _size: U64 = 0
  var _array: Array[((Key, Value) | _MapEmpty | _MapDeleted)] =
    Array[((Key, Value) | _MapEmpty | _MapDeleted)].prealloc(8)

  new create() =>
    for i in Range(0, 8) do
      _array.append(_MapEmpty)
    end

  fun box size(): U64 => _size

  fun box space(): U64 => _array.space()

  fun box apply(key: Key): this->Value ? =>
   var (index, found) = _search(key)

   if found then
     match _array(index)
     | (let k: Key, let v: this->Value!) =>
       return consume v
     end
   end
   error

  fun ref update(key: Key, value: Value): (Value^ | None) =>
    try
      let (index, found) = _search(key)
      let prev = _array(index) = (key, consume value)

      match consume prev
      | (let k: Key, let v: Value) =>
        return consume v
      else
        _size = _size + 1

        if (_size * 2) > _array.size() then
          _resize()
        end
      end
    end
    None

  fun ref remove(key: Key): (Value^ | None) =>
    try
      let (index, found) = _search(key)

      if found then
        let prev = _array(index) = _MapDeleted
        _size = _size - 1

        match consume prev
        | (let k: Key, let v: Value) =>
          return consume v
        end
      end
    end
    None

  fun box _search(key: Key): (U64, Bool) =>
    let len: U64 = _array.size()
    var index_del = _array.size()
    let mask = index_del - 1
    let h = key.hash()
    var index = h and mask

    try
      for i in Range(0, len) do
        let entry = _array(i)

        match entry
        | (let k: Key, let v: this->Value!) =>
          if k == key then
            return (i, true)
          end
        | _MapEmpty =>
          if index_del <= mask then
            return (index_del, false)
          else
            return (i, false)
          end
        | _MapDeleted =>
          if index_del > mask then
            index_del = i
          end
        end

        index = (h + ((i + (i * i)) / 2)) and mask;
      end
    end

    (index_del, false)

  fun ref _resize() =>
    let _old = _array
    let old_len = _old.size()
    let new_len = old_len * 4

    _array = Array[((Key, Value) | _MapEmpty | _MapDeleted)].prealloc(new_len)
    _size = 0

    for i in Range(0, new_len) do
      _array.append(_MapEmpty)
    end

    try
      for i in Range(0, old_len) do
        let entry = _old(i) = _MapDeleted

        match consume entry
        | (let k: Key, let v: Value) =>
          this(k) = consume v
        end
      end
    end

// class MapKeys[
//   Key: (Hashable box & Comparable[Key] box),
//   Value,
//   M: Map[Key, Value] box] is Iterator[Key]
//
//   var _map: M
//   var _i: U64
//
//   new create(map: M) =>
//     _map = map
//     _i = 0
//
//   fun box has_next(): Bool => _i < _map.size()
//
//   fun ref next(): Key =>
//     if _i < _map.size() then
//       _i = _i + 1
//     else
//       _i
//     end
