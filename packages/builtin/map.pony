trait Hashable
  fun box hash(): U64

primitive _MapEmpty is Comparable[_MapEmpty]
primitive _MapDeleted is Comparable[_MapDeleted]

class Map[Key: (Hashable val & Comparable[Key] val), Value]
  var _count: U64 = 0
  var _array: Array[((Key, Value) | _MapEmpty | _MapDeleted)] =
    Array[((Key, Value) | _MapEmpty | _MapDeleted)].init(_MapEmpty, 8)

  fun box size(): U64 => _count

  fun box apply(key: Key): this->Value ? =>
    if _count == 0 then
      error
    end

    var (pos, entry) = _search(key)

    match entry
    | var value: this->Value => value
    else
      error
    end

  fun ref update(key: Key, value: Value): (Value | None) =>
    var (pos, entry) = _search(key)

    try
      _array(pos) = (key, value)

      match entry
      | var prev: Value => value
      else
        _count = _count + 1

        if (_count << 1) > _array.length() then
          _resize()
        end

        None
      end
    else
      None
    end

  fun ref remove(key: Key): (Value | None) =>
    if _count == 0 then
      return None
    end

    var (pos, entry) = _search(key)

    match entry
    | var prev: Value =>
      try
        _array(pos) = _MapDeleted
        _count = _count - 1
      end
      prev
    else
      None
    end

  fun box _search(key: Key): (U64, (this->Value | _MapEmpty | _MapDeleted)) =>
    let len: U64 = _array.length()
    var index_del = _array.length()
    var mask = index_del - 1
    var h = key.hash()
    var index = h and mask

    try
      var i: U64 = 0

      while i < len do
        var entry = _array(i)

        match entry
        | (var k: Key, var v: this->Value) =>
          if k == key then
            return (i, v)
          end
        | _MapEmpty =>
          if index_del <= mask then
            return (index_del, _MapEmpty)
          else
            return (i, _MapEmpty)
          end
        | _MapDeleted =>
          if index_del > mask then
            index_del = i
          end
        end

        index = (h + ((i + (i * i)) >> 1)) and mask;
        i = i + 1
      end
    end

    (index_del, _MapDeleted)

  fun ref _resize() =>
    var _old = _array
    let old_len = _old.length()

    _array = Array[((Key, Value) | _MapEmpty | _MapDeleted)]
      .init(_MapEmpty, old_len << 2)
    _count = 0

    try
      var i: U64 = 0
      while i < old_len do
        var entry = _old(i)

        match entry
        | (var k: Key, var v: Value) =>
          this(k) = v
        end

        i = i + 1
      end
    end
