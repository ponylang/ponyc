trait Hashable
  fun box hash(): U64

primitive _MapEmpty is Comparable[_MapEmpty]
primitive _MapDeleted is Comparable[_MapDeleted]

class Map[Key: (Hashable val & Comparable[Key] val), Value]
  var _count: U64 = 0
  var _array: Array[((Key, Value) | _MapEmpty | _MapDeleted)] =
    Array[((Key, Value) | _MapEmpty | _MapDeleted)].prealloc(8)

  new create() =>
    for i in Range(0, 8) do
      _array.append(_MapEmpty)
    end

  fun box size(): U64 => _count

  // TODO:
  //fun box apply(key: Key): this->Value ? =>
  //  var (index, found) = _search(key)
  //
  //  if found then
  //    match _array(index)
  //    | (var k: Key, var v: this->Value) =>
  //      return v
  //    end
  //  end
  //  error

  fun ref update(key: Key, value: Value): (Value^ | None) =>
    try
      var (index, found) = _search(key)
      var prev = _array(index) = (key, consume value)

      match consume prev
      | (var k: Key, var v: Value) =>
        return consume v
      else
        _count = _count + 1

        if (_count * 2) > _array.length() then
          _resize()
        end
      end
    end
    None

  fun ref remove(key: Key): (Value^ | None) =>
    try
      var (index, found) = _search(key)

      if found then
        var prev = _array(index) = _MapDeleted
        _count = _count - 1

        match consume prev
        | (var k: Key, var v: Value) =>
          return consume v
        end
      end
    end
    None

  fun box _search(key: Key): (U64, Bool) =>
    let len: U64 = _array.length()
    var index_del = _array.length()
    var mask = index_del - 1
    var h = key.hash()
    var index = h and mask

    try
      for i in Range(0, len) do
        var entry = _array(i)

        match entry
        | (var k: Key, var v: Any) =>
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
    var _old = _array
    let old_len = _old.length()
    let new_len = old_len * 4

    _array = Array[((Key, Value) | _MapEmpty | _MapDeleted)].prealloc(new_len)
    _count = 0

    for i in Range(0, new_len) do
      _array.append(_MapEmpty)
    end

    try
      for i in Range(0, old_len) do
        var entry = _old(i) = _MapDeleted

        match consume entry
        | (var k: Key, var v: Value) =>
          this(k) = consume v
        end
      end
    end
