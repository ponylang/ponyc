use "collections"

class \nodoc\ iso _SmallGeneratorBoundsProperty is Property[U64]
  fun name(): String => "Gen/size/small_bounds"
  fun gen(): Generator[U64] => Generators.u64()

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50, max_shrink_reductions' = 16)

  fun property(seed: U64, h: PropertyHelper) ? =>
    for from in Range[USize].till(0, 5) do
      for to in Range[USize].till(0, 5) do
        _GeneratorBounds.all(h, seed, from, to)?
      end
    end

class \nodoc\ iso _LargeGeneratorBoundsProperty is Property[U64]
  fun name(): String => "Gen/size/large_bounds"
  fun gen(): Generator[U64] => Generators.u64()

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 12, max_shrink_reductions' = 16)

  fun property(seed: U64, h: PropertyHelper) ? =>
    let rnd = Randomness(seed)
    let first = Generators.frequency[USize](
      [(7, Generators.usize(0, 4)); (3, Generators.usize(5, 300))])
        .generate(rnd)?
    let second = rnd.usize(5, 300)
    if rnd.bool() then
      _GeneratorBounds.all(h, seed, first, second)?
    else
      _GeneratorBounds.all(h, seed, second, first)?
    end
    _GeneratorBounds.all(h, seed, 300, 300)?

class \nodoc\ iso _GeneratorDomainProperty is Property[U64]
  fun name(): String => "Gen/size/small_domains"
  fun gen(): Generator[U64] => Generators.u64()

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 12, max_shrink_reductions' = 16)

  fun property(seed: U64, h: PropertyHelper) ? =>
    for domain in Range[USize].till(1, 7) do
      for from in Range[USize].till(0, 5) do
        for to in Range[USize].till(0, 5) do
          _GeneratorBounds.associative(h, seed, from, to, domain)?
        end
      end
      _GeneratorBounds.associative(h, seed, domain, domain, domain)?
      _GeneratorBounds.associative(h, seed, domain + 1, domain + 1, domain)?
    end
    _GeneratorBounds.identities(h, seed)?

    let reset: Array[USize] val =
      recover val
        let keys = Array[USize](201)
        for _ in Range[USize].till(0, 100) do keys.push(0) end
        for _ in Range[USize].till(0, 100) do keys.push(1) end
        keys .> push(2)
      end
    _GeneratorBounds.associative(
      h, seed, 3, 3, 3
      where schedule = reset, expected_draws = 201)?
    let duplicates = recover val Array[USize].init(0, 102) end
    _GeneratorBounds.associative(
      h, seed, 2, 2, 1
      where schedule = duplicates, expected_draws = 101)?
    _GeneratorBounds.associative(
      h, seed, 1, 2, 1
      where schedule = duplicates, expected_draws = 101)?
    let beyond_max: Array[USize] val = [0; 1; 0]
    _GeneratorBounds.associative(
      h, seed, 2, 2, 2
      where schedule = beyond_max, expected_draws = 2)?

class \nodoc\ iso _LegacyGeneratorBoundsProperty is Property[U64]
  fun name(): String => "Gen/size/extended_bounds"
  fun gen(): Generator[U64] => Generators.u64()

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 1, max_shrink_reductions' = 16)

  fun property(seed: U64, h: PropertyHelper) ? =>
    _GeneratorBounds.sequences(h, seed, 0, 10)?
    _GeneratorBounds.sequences(h, seed, 10, 5)?
    _GeneratorBounds.sequences(h, seed, 1, 100)?
    _GeneratorBounds.associative(h, seed, 1, 100, 100)?
    let set = Generators.set_of[U8](Generators.u8(), 0, 1024)
      .generate(Randomness(seed))?
    h.assert_true(set.size() <= 256, "set_of: full U8 domain has 256 values")
    for to in Range[USize].till(1, 255) do
      _GeneratorBounds.associative(h, seed, 0, to, 256)?
    end
    for to in Range[USize].till(1, 50) do
      _GeneratorBounds.sequences(h, seed, 0, to)?
    end

primitive \nodoc\ _GeneratorBounds
  fun all(h: PropertyHelper, seed: U64, from: USize, to: USize) ? =>
    sequences(h, seed, from, to)?
    associative(h, seed, from, to, from.max(to).max(1))?
    strings(h, seed, from, to)?

  fun sequences(
    h: PropertyHelper,
    seed: U64,
    from: USize,
    to: USize,
    expected: (USize | None) = None,
    replay: (Array[_Choice val] val | None) = None)
    ?
  =>
    let bounds = _bounds_text(from, to, seed)
    let gen = Generators.usize(400, 500)
    let lo =
      match expected
      | let n: USize => n
      else from.min(to)
      end
    let hi =
      match expected
      | let n: USize => n
      else from.max(to)
      end
    let array_label: String val = "array_of" + bounds
    var rnd = _randomness(seed, replay)
    let array = _draw(h, array_label, Generators.array_of(gen, from, to), rnd)?
    _assert_bound_draws(h, array_label, rnd, replay, hi)
    _sequence(
      h, array_label, lo, hi, array.size(), array.values(), replay)?

    let iso_array_label: String val = "iso_seq_of" + bounds
    rnd = _randomness(seed, replay)
    let iso_array: Array[USize] val =
      _draw(
        h,
        iso_array_label,
        Generators.iso_seq_of[USize, Array[USize] iso](gen, from, to),
        rnd)?
    _assert_bound_draws(h, iso_array_label, rnd, replay, hi)
    _sequence(
      h,
      iso_array_label,
      lo,
      hi,
      iso_array.size(),
      iso_array.values(),
      replay)?

    let seq_label: String val = "seq_of" + bounds
    rnd = _randomness(seed, replay)
    let seq =
      _draw(
        h,
        seq_label,
        Generators.seq_of[USize, Array[USize]](gen, from, to),
        rnd)?
    _assert_bound_draws(h, seq_label, rnd, replay, hi)
    _sequence(
      h, seq_label, lo, hi, seq.size(), seq.values(), replay)?

    let list_label: String val = "list_of" + bounds
    rnd = _randomness(seed, replay)
    let list = _draw(h, list_label, Generators.list_of(gen, from, to), rnd)?
    _assert_bound_draws(h, list_label, rnd, replay, hi)
    _sequence(
      h, list_label, lo, hi, list.size(), list.values(), replay)?

    let vec_label: String val = "vec_of" + bounds
    rnd = _randomness(seed, replay)
    let vec = _draw(h, vec_label, Generators.vec_of(gen, from, to), rnd)?
    _assert_bound_draws(h, vec_label, rnd, replay, hi)
    _sequence(
      h, vec_label, lo, hi, vec.size(), vec.values(), replay)?

    let plist_label: String val = "persistent_list_of" + bounds
    rnd = _randomness(seed, replay)
    let plist =
      _draw(
        h,
        plist_label,
        Generators.persistent_list_of(gen, from, to),
        rnd)?
    _assert_bound_draws(h, plist_label, rnd, replay, hi)
    _sequence(
      h, plist_label, lo, hi, plist.size(), plist.values(), replay)?

  fun associative(
    h: PropertyHelper,
    seed: U64,
    from: USize,
    to: USize,
    domain: USize,
    expected: (USize | None) = None,
    schedule: (Array[USize] val | None) = None,
    expected_draws: USize = 0)
    ?
  =>
    let numbers = _number_keys(domain, schedule)
    let pool = _token_pool(domain)
    let tokens = _identity_keys(pool, numbers)
    let number_pairs = _indexed_pairs[USize](numbers, {(i) => i })
    let token_pairs = _indexed_pairs[String](numbers, {(i) ? => pool(i)? })
    let choices: (Array[_Choice val] val | None) =
      match schedule
      | let keys: Array[USize] val =>
        recover val Array[_Choice val].init(_BoolChoice(true), keys.size()) end
      end
    let lo =
      match expected
      | let n: USize => n
      else from.min(to)
      end
    let hi =
      match expected
      | let n: USize => n
      else from.max(to).min(domain)
      end
    let context: String val =
      _bounds_text(from, to, seed) + ", domain=" + domain.string()
    _AssociativeGeneratorCheck(
      h, seed, context, lo, hi, choices, expected_draws)
      .> apply(
        "set_of",
        Generators.set_of[USize](numbers, from, to),
        {(result): (USize, Bool) ? =>
          (result.size(),
            _GeneratorBounds._number_members(result.values(), domain)?)
        })?
      .> apply(
        "set_is_of",
        Generators.set_is_of[String](tokens, from, to),
        {(result): (USize, Bool) =>
          (result.size(),
            _GeneratorBounds._token_members(result.values(), pool))
        })?
      .> apply(
        "map_of",
        Generators.map_of[USize, USize](number_pairs, from, to),
        {(result): (USize, Bool) ? =>
          (result.size(),
            _GeneratorBounds._number_entries(result.pairs(), domain)?)
        })?
      .> apply(
        "map_is_of",
        Generators.map_is_of[String, USize](token_pairs, from, to),
        {(result): (USize, Bool) ? =>
          (result.size(),
            _GeneratorBounds._token_entries(result.pairs(), pool)?)
        })?
      .> apply(
        "persistent_set_of",
        Generators.persistent_set_of[USize](numbers, from, to),
        {(result): (USize, Bool) ? =>
          (result.size(),
            _GeneratorBounds._number_members(result.values(), domain)?)
        })?
      .> apply(
        "persistent_set_is_of",
        Generators.persistent_set_is_of[String](tokens, from, to),
        {(result): (USize, Bool) =>
          (result.size(),
            _GeneratorBounds._token_members(result.values(), pool))
        })?
      .> apply(
        "persistent_map_of",
        Generators.persistent_map_of[USize, USize](number_pairs, from, to),
        {(result): (USize, Bool) ? =>
          (result.size(),
            _GeneratorBounds._number_entries(result.pairs(), domain)?)
        })?
      .> apply(
        "persistent_map_is_of",
        Generators.persistent_map_is_of[String, USize](token_pairs, from, to),
        {(result): (USize, Bool) ? =>
          (result.size(),
            _GeneratorBounds._token_entries(result.pairs(), pool)?)
        })?

    None

  fun strings(
    h: PropertyHelper,
    seed: U64,
    from: USize,
    to: USize,
    byte_replay: (Array[_Choice val] val | None) = None,
    rune_replay: (Array[_Choice val] val | None) = None)
    ?
  =>
    let bounds = _bounds_text(from, to, seed)
    let lo = from.min(to)
    let hi = from.max(to)
    let byte_label: String val = "byte_string" + bounds
    var rnd = _randomness(seed, byte_replay)
    let bytes =
      _draw(
        h,
        byte_label,
        Generators.byte_string(Generators.u8(128, 200), from, to),
        rnd)?
    _assert_bound_draws(h, byte_label, rnd, byte_replay, hi)
    _size(h, byte_label, bytes.size(), lo, hi)
    var valid = true
    for byte in bytes.values() do
      valid = valid and (byte >= 128) and (byte <= 200)
    end
    h.assert_true(valid, byte_label + ": byte outside 128..200")

    _ascii(
      h,
      "ascii" + bounds,
      Generators.ascii(from, to, ASCIINUL),
      ASCIINUL(),
      _randomness(seed, byte_replay),
      lo,
      hi,
      byte_replay)?
    _ascii(
      h,
      "ascii_printable" + bounds,
      Generators.ascii_printable(from, to),
      ASCIIPrintable(),
      _randomness(seed, byte_replay),
      lo,
      hi,
      byte_replay)?
    _ascii(
      h,
      "ascii_numeric" + bounds,
      Generators.ascii_numeric(from, to),
      ASCIIDigits(),
      _randomness(seed, byte_replay),
      lo,
      hi,
      byte_replay)?
    _ascii(
      h,
      "ascii_letters" + bounds,
      Generators.ascii_letters(from, to),
      ASCIILetters(),
      _randomness(seed, byte_replay),
      lo,
      hi,
      byte_replay)?

    let utf32_label: String val = "utf32_codepoint_string" + bounds
    let utf32_source =
      Generators.utf32_codepoint_string(
        Generators.u32(0x10000, 0x10FFFF), from, to)
    rnd = _randomness(seed, rune_replay)
    let utf32 = _draw(h, utf32_label, utf32_source, rnd)?
    _assert_bound_draws(h, utf32_label, rnd, rune_replay, hi)
    _unicode(h, utf32_label, utf32, 0x10000, 0x10FFFF, lo, hi)?

    let unicode_label: String val = "unicode" + bounds
    rnd = Randomness(seed)
    let unicode = _draw(h, unicode_label, Generators.unicode(from, to), rnd)?
    _unicode(h, unicode_label, unicode, 0, 0x10FFFF, lo, hi)?
    _assert_closed(h, unicode_label, rnd)

    let bmp_label: String val = "unicode_bmp" + bounds
    rnd = Randomness(seed)
    let bmp = _draw(h, bmp_label, Generators.unicode_bmp(from, to), rnd)?
    _unicode(h, bmp_label, bmp, 0, 0xFFFF, lo, hi)?
    _assert_closed(h, bmp_label, rnd)

  fun identities(h: PropertyHelper, seed: U64) ? =>
    let key = "key"
    let same = Generators.unit[String](key)
    let equal = Generators.repeatedly[String]({(): String^ => "key".clone() })
    let same_pairs = _pairs[String](same)
    let equal_pairs = _pairs[String](equal)
    let rnd = Randomness(seed)
    let draws: Array[_Choice val] val =
      recover val [_BoolChoice(true); _BoolChoice(true); _BoolChoice(false)] end
    rnd._replay(draws)
    let set_is = Generators.set_is_of[String](same, 0, 100).generate(rnd)?
    _assert_replayed(h, "set_is_of", rnd, draws.size())
    h.assert_eq[USize](1, set_is.size(), "set_is_of repeated reference")
    rnd._replay(draws)
    let persistent_set_is =
      Generators.persistent_set_is_of[String](same, 0, 100).generate(rnd)?
    _assert_replayed(h, "persistent_set_is_of", rnd, draws.size())
    h.assert_eq[USize](
      1,
      persistent_set_is.size(),
      "persistent_set_is_of repeated reference")
    let map_draws: Array[_Choice val] val =
      [ _BoolChoice(true)
        _IntChoice(42, 42, 43)
        _BoolChoice(true)
        _IntChoice(43, 42, 43)
        _BoolChoice(false)
      ]
    rnd._replay(map_draws)
    let map_is = Generators.map_is_of[String, USize](same_pairs, 0, 100)
      .generate(rnd)?
    _assert_replayed(h, "map_is_of", rnd, map_draws.size())
    h.assert_eq[USize](1, map_is.size(), "map_is_of repeated reference")
    h.assert_eq[USize](43, map_is(key)?, "map_is_of replaces the value")
    rnd._replay(map_draws)
    let persistent_map_is =
      Generators.persistent_map_is_of[String, USize](same_pairs, 0, 100)
        .generate(rnd)?
    _assert_replayed(h, "persistent_map_is_of", rnd, map_draws.size())
    h.assert_eq[USize](
      1,
      persistent_map_is.size(),
      "persistent_map_is_of repeated reference")
    h.assert_eq[USize](
      43, persistent_map_is(key)?, "persistent_map_is_of replaces the value")
    rnd._replay(map_draws)
    let map = Generators.map_of[String, USize](equal_pairs, 0, 100)
      .generate(rnd)?
    _assert_replayed(h, "map_of", rnd, map_draws.size())
    h.assert_eq[USize](1, map.size(), "map_of separately allocated equal keys")
    h.assert_eq[USize](43, map(key)?, "map_of replaces the value")
    rnd._replay(map_draws)
    let persistent_map =
      Generators.persistent_map_of[String, USize](equal_pairs, 0, 100)
        .generate(rnd)?
    _assert_replayed(h, "persistent_map_of", rnd, map_draws.size())
    h.assert_eq[USize](
      1,
      persistent_map.size(),
      "persistent_map_of separately allocated equal keys")
    h.assert_eq[USize](
      43, persistent_map(key)?, "persistent_map_of replaces the value")

  fun _number_keys(domain: USize, schedule: (Array[USize] val | None))
    : Generator[USize]
  =>
    Generator[USize](
      object is GenObj[USize]
        let _schedule: (Array[USize] val | None) = schedule
        fun generate(rnd: Randomness): USize ? =>
          match _schedule
          | let keys: Array[USize] val => keys(rnd._consumed() - 1)?
          else
            rnd._choices_size() % domain
          end
      end)

  fun _token_pool(domain: USize): Array[String] val =>
    recover val
      let result = Array[String](domain)
      for _ in Range[USize].till(0, domain) do
        result.push("key".clone())
      end
      result
    end

  fun _identity_keys(keys: Array[String] val, indices: Generator[USize])
    : Generator[String]
  =>
    Generator[String](
      object is GenObj[String]
        let _keys: Array[String] val = keys
        let _indices: Generator[USize] = indices
        fun generate(rnd: Randomness): String ? =>
          _keys(_indices.generate(rnd)?)?
      end)

  fun _indexed_pairs[K: Any val](
    indices: Generator[USize],
    key: {(USize): K ?} box)
    : Generator[(K, USize)]
  =>
    Generator[(K, USize)](
      object is GenObj[(K, USize)]
        let _indices: Generator[USize] = indices
        let _key: {(USize): K ?} box = key
        fun generate(rnd: Randomness): (K, USize) ? =>
          let index = _indices.generate(rnd)?
          (_key(index)?, index + 42)
      end)

  fun _pairs[K: Any val](keys: Generator[K])
    : Generator[(K, USize)]
  =>
    Generator[(K, USize)](
      object is GenObj[(K, USize)]
        let _keys: Generator[K] = keys
        fun generate(rnd: Randomness): (K, USize) ? =>
          (_keys.generate(rnd)?, rnd.usize(42, 43))
      end)

  fun _randomness(seed: U64, choices: (Array[_Choice val] val | None))
    : Randomness
  =>
    let rnd = Randomness(seed)
    match choices
    | let replay: Array[_Choice val] val => rnd._replay(replay)
    end
    rnd

  fun _assert_bound_draws(
    h: PropertyHelper,
    family: String,
    rnd: Randomness,
    replay: (Array[_Choice val] val | None),
    upper: USize)
  =>
    if replay is None then
      _assert_closed(h, family, rnd)
    else
      _assert_replayed(h, family, rnd, upper * 2)
    end

  fun _assert_replayed(
    h: PropertyHelper,
    family: String,
    rnd: Randomness,
    expected: USize)
  =>
    h.assert_eq[USize](expected, rnd._consumed(), family + ": replay choices")
    _assert_closed(h, family, rnd)

  fun _assert_closed(h: PropertyHelper, family: String, rnd: Randomness) =>
    h.assert_false(
      rnd._replay_exhausted(), family + ": replay fell back to fresh draws")
    try
      rnd.end_span()?
      h.fail(family + ": left a span open")
    end

  fun _number_members(values: Iterator[USize], domain: USize): Bool ? =>
    while values.has_next() do
      if values.next()? >= domain then return false end
    end
    true

  fun _token_members(values: Iterator[String], pool: Array[String] val)
    : Bool
  =>
    for key in values do
      if not pool.contains(key) then return false end
    end
    true

  fun _number_entries(pairs: Iterator[(USize, USize)], domain: USize)
    : Bool ?
  =>
    while pairs.has_next() do
      (let key, let value) = pairs.next()?
      if (key >= domain) or (value != (key + 42)) then return false end
    end
    true

  fun _token_entries(pairs: Iterator[(String, USize)], pool: Array[String] val)
    : Bool ?
  =>
    while pairs.has_next() do
      (let key, let value) = pairs.next()?
      if (value < 42) or ((value - 42) >= pool.size()) then return false end
      if key isnt pool(value - 42)? then return false end
    end
    true

  fun _draw[A](
    h: PropertyHelper,
    family: String,
    source: Generator[A],
    rnd: Randomness)
    : A^ ?
  =>
    try source.generate(rnd)?
    else
      h.fail(family + ": generation returned an error")
      error
    end

  fun _sequence(
    h: PropertyHelper,
    family: String,
    lo: USize,
    hi: USize,
    size: USize,
    values: Iterator[USize],
    replay: (Array[_Choice val] val | None))
    ?
  =>
    _size(h, family, size, lo, hi)
    var valid = true
    var index: USize = 0
    while values.has_next() do
      let value = values.next()?
      match replay
      | let choices: Array[_Choice val] val =>
        match choices((index * 2) + 1)?
        | let draw: _IntChoice =>
          h.assert_eq[USize](
            draw.value.usize(),
            value,
            family + ": element at index " + index.string())
        else error
        end
      else
        valid = valid and (value >= 400) and (value <= 500)
      end
      index = index + 1
    end
    h.assert_true(valid, family + ": element outside 400..500")

  fun _ascii(
    h: PropertyHelper,
    family: String,
    source: Generator[String],
    range: String,
    rnd: Randomness,
    lo: USize,
    hi: USize,
    replay: (Array[_Choice val] val | None))
    ?
  =>
    let value = _draw(h, family, source, rnd)?
    _assert_bound_draws(h, family, rnd, replay, hi)
    _size(h, family, value.size(), lo, hi)
    let allowed = Array[Bool].init(false, 128)
    for byte in range.values() do
      allowed.update(byte.usize(), true)?
    end
    var valid = true
    for byte in value.values() do
      valid = valid and (byte < 128) and allowed(byte.usize())?
    end
    h.assert_true(valid, family + ": byte outside the supplied range")

  fun _unicode(
    h: PropertyHelper,
    family: String,
    value: String box,
    min: U32,
    max: U32,
    lo: USize,
    hi: USize)
    ?
  =>
    var count: USize = 0
    var valid = true
    let encoded = String(value.size())
    let runes = value.runes()
    while runes.has_next() do
      let rune = runes.next()?
      valid =
        valid and (rune >= min) and (rune <= max) and
          ((rune < 0xD800) or (rune > 0xDFFF))
      encoded.push_utf32(rune)
      count = count + 1
    end
    _size(h, family, count, lo, hi)
    h.assert_true(valid, family + ": code point outside the supplied range")
    h.assert_true(encoded == value, family + ": malformed UTF-8")

  fun _size(
    h: PropertyHelper,
    family: String,
    size: USize,
    lo: USize,
    hi: USize)
  =>
    let actual = family + ": size " + size.string()
    h.assert_true(size >= lo, actual + " is below " + lo.string())
    h.assert_true(size <= hi, actual + " is above " + hi.string())

  fun _bounds_text(from: USize, to: USize, seed: U64): String =>
    "(from=" + from.string() + ", to=" + to.string() + ", seed=" +
      seed.string() + ")"

class \nodoc\ val _AssociativeGeneratorCheck
  let _h: PropertyHelper
  let _seed: U64
  let _context: String
  let _lo: USize
  let _hi: USize
  let _choices: (Array[_Choice val] val | None)
  let _expected_draws: USize

  new val create(
    h: PropertyHelper,
    seed: U64,
    context: String,
    lo: USize,
    hi: USize,
    choices: (Array[_Choice val] val | None),
    expected_draws: USize)
  =>
    _h = h
    _seed = seed
    _context = context
    _lo = lo
    _hi = hi
    _choices = choices
    _expected_draws = expected_draws

  fun apply[A](
    family: String,
    source: Generator[A],
    inspect: {(A): (USize, Bool) ?} box)
    ?
  =>
    let label: String val = family + _context
    let rnd = _GeneratorBounds._randomness(_seed, _choices)
    if _choices is None then rnd._start_recording() end
    let result =
      try source.generate(rnd)?
      else
        _check_draws(label, rnd)
        _h.assert_true(_lo > _hi, label + ": errored with a sufficient domain")
        return
      end
    _check_draws(label, rnd)
    _h.assert_true(_lo <= _hi, label + ": succeeded with insufficient domain")
    if _lo <= _hi then
      (let size, let valid) = inspect(consume result)?
      _GeneratorBounds._size(_h, label, size, _lo, _hi)
      _h.assert_true(valid, label + ": contents differ from the source")
    end

  fun _check_draws(label: String, rnd: Randomness) =>
    if _expected_draws > 0 then
      _GeneratorBounds._assert_replayed(_h, label, rnd, _expected_draws)
    else
      _GeneratorBounds._assert_closed(_h, label, rnd)
    end

class \nodoc\ iso _GeneratorContentProperty is Property[U64]
  fun name(): String => "Gen/size/content"
  fun gen(): Generator[U64] => Generators.u64()
  fun params(): PropertyParams => PropertyParams(where num_samples' = 5)

  fun property(seed: U64, h: PropertyHelper) ? =>
    let rnd = Randomness(seed)
    let start = rnd.usize(400, 498).i128()
    let list_draws = _draws([start; start + 1; start + 2])
    rnd._replay(list_draws)
    let list = Generators.persistent_list_of(Generators.usize(400, 500), 3, 3)
      .generate(rnd)?
    _GeneratorBounds._assert_replayed(
      h, "persistent_list_of", rnd, list_draws.size())
    h.assert_eq[USize](start.usize(), list(0)?, "first persistent list draw")
    h.assert_eq[USize](
      (start + 1).usize(),
      list(1)?,
      "second persistent list draw")
    h.assert_eq[USize](
      (start + 2).usize(),
      list(2)?,
      "third persistent list draw")

    let byte_draws = _draws([0; 255; 128])
    rnd._replay(byte_draws)
    let bytes = Generators.byte_string(Generators.u8(), 3, 3).generate(rnd)?
    _GeneratorBounds._assert_replayed(h, "byte_string", rnd, byte_draws.size())
    h.assert_eq[U8](0, bytes(0)?, "byte_string preserves NUL")
    h.assert_eq[U8](255, bytes(1)?, "byte_string preserves 0xff")
    h.assert_eq[U8](128, bytes(2)?, "byte_string preserves 0x80")

    _ascii_endpoints(
      h,
      rnd,
      "ascii",
      Generators.ascii(1, 1, ASCIIAllWithNUL),
      ASCIIAllWithNUL())?
    _ascii_endpoints(
      h,
      rnd,
      "ascii_printable",
      Generators.ascii_printable(1, 1),
      ASCIIPrintable())?
    _ascii_endpoints(
      h,
      rnd,
      "ascii_numeric",
      Generators.ascii_numeric(1, 1),
      ASCIIDigits())?
    _ascii_endpoints(
      h,
      rnd,
      "ascii_letters",
      Generators.ascii_letters(1, 1),
      ASCIILetters())?

    let scalars: Array[I128] val =
      [0; 0x7F; 0x80; 0x7FF; 0x800; 0xD7FF; 0xE000; 0xFFFF; 0x10000; 0x10FFFF]
    let rune_draws = _draws(scalars)
    rnd._replay(rune_draws)
    let utf32 = Generators.utf32_codepoint_string(
      Generators.u32(), scalars.size(), scalars.size()).generate(rnd)?
    _GeneratorBounds._assert_replayed(
      h, "utf32_codepoint_string", rnd, rune_draws.size())
    let runes = utf32.runes()
    for scalar in scalars.values() do
      h.assert_eq[U32](scalar.u32(), runes.next()?, "UTF-32 input scalar")
    end
    h.assert_false(runes.has_next(), "UTF-32 output has extra scalars")

    _unicode_endpoints(h, rnd, "unicode", Generators.unicode(1, 1), 0x10FFFF)?
    _unicode_endpoints(
      h, rnd, "unicode_bmp", Generators.unicode_bmp(1, 1), 0xFFFF)?

    // These seeds straddle the split when the second weight shifts by one.
    _unicode_weight(h, "unicode", Generators.unicode(1, 1), 2725654, 552411)?
    _unicode_weight(
      h, "unicode_bmp", Generators.unicode_bmp(1, 1), 98422, 7144)?

    _GeneratorBounds.sequences(h, seed, 2, 2 where replay = list_draws)?
    _GeneratorBounds.strings(
      h, seed, 2, 2
      where byte_replay = _draws([128; 140; 160]),
        rune_replay = _draws([0x10000; 0x10100; 0x10200]))?
    _invalid_scalars(h, rnd)?
    _retry_reset(h, rnd)?

  fun _invalid_scalars(h: PropertyHelper, rnd: Randomness) ? =>
    let source = Generators.utf32_codepoint_string(Generators.u32(), 1, 1)
    for invalid in [I128(0xD800); I128(0xDFFF); I128(0x110000)].values() do
      rnd._replay(_retry_draws(invalid, 99))
      h.assert_eq[U32](
        0x10000,
        source.generate(rnd)?.runes().next()?,
        "UTF-32 recovers after 99 invalid scalars")
      _GeneratorBounds._assert_replayed(h, "utf32_codepoint_string", rnd, 101)
      rnd._replay(_retry_draws(invalid, 100))
      let rejected = try source.generate(rnd)?; false else true end
      h.assert_true(rejected, "UTF-32 rejects 100 consecutive invalid scalars")
      h.assert_eq[USize](101, rnd._consumed(), "UTF-32 leaves sentinel unread")
      _GeneratorBounds._assert_closed(h, "utf32_codepoint_string", rnd)
    end

  fun _retry_reset(h: PropertyHelper, rnd: Randomness) ? =>
    let first = _retry_draws(0xD800, 99)
    let choices: Array[_Choice val] val =
      recover val
        let script = Array[_Choice val](first.size() + 3)
        script.append(first)
        script.push(_BoolChoice(true))
        script.push(_IntChoice(0xDFFF, 0, U32.max_value().i128()))
        script .> push(_IntChoice(0x10FFFF, 0, U32.max_value().i128()))
      end
    rnd._replay(choices)
    let generated = Generators.utf32_codepoint_string(Generators.u32(), 2, 2)
      .generate(rnd)?
    _GeneratorBounds._assert_replayed(
      h, "utf32_codepoint_string", rnd, choices.size())
    let runes = generated.runes()
    h.assert_eq[U32](0x10000, runes.next()?, "UTF-32 scalar before retry reset")
    h.assert_eq[U32](0x10FFFF, runes.next()?, "UTF-32 scalar after retry reset")
    h.assert_false(runes.has_next(), "UTF-32 has extra scalars after reset")

  fun _retry_draws(invalid: I128, count: USize): Array[_Choice val] val =>
    recover val
      let choices = Array[_Choice val](count + 2)
      choices.push(_BoolChoice(true))
      for _ in Range[USize].till(0, count) do
        choices.push(_IntChoice(invalid, 0, U32.max_value().i128()))
      end
      choices .> push(_IntChoice(0x10000, 0, U32.max_value().i128()))
    end

  fun _draws(values: Array[I128] val): Array[_Choice val] val =>
    recover val
      let choices = Array[_Choice val](values.size() * 2)
      for value in values.values() do
        choices.push(_BoolChoice(true))
        choices.push(_IntChoice(value, 0, I128.max_value()))
      end
      choices
    end

  fun _ascii_endpoints(
    h: PropertyHelper,
    rnd: Randomness,
    family: String,
    source: Generator[String],
    repertoire: String)
    ?
  =>
    for (choice, index) in
      [(I128(0), USize(0)); (I128.max_value(), repertoire.size() - 1)].values()
    do
      rnd._replay(_draws([choice]))
      let actual = source.generate(rnd)?
      _GeneratorBounds._assert_replayed(h, family, rnd, 2)
      h.assert_eq[U8](
        repertoire(index)?,
        actual(0)?,
        family + ": byte at repertoire index " + index.string())
    end

  fun _unicode_endpoints(
    h: PropertyHelper,
    rnd: Randomness,
    family: String,
    source: Generator[String],
    max: U32)
    ?
  =>
    for (selection, scalar) in
      [ (I128(1), U32(0))
        (I128(1), U32(0xD7FF))
        (I128.max_value(), U32(0xE000))
        (I128.max_value(), max)
      ].values()
    do
      rnd._replay(recover val
        [ _BoolChoice(true)
          _IntChoice(selection, 0, I128.max_value())
          _IntChoice(scalar.i128(), 0, I128.max_value())
        ]
      end)
      let actual = source.generate(rnd)?.runes().next()?
      _GeneratorBounds._assert_replayed(h, family, rnd, 3)
      h.assert_eq[U32](scalar, actual, family + ": repertoire endpoint")
    end

  fun _unicode_weight(
    h: PropertyHelper,
    family: String,
    source: Generator[String],
    lower_seed: U64,
    upper_seed: U64)
    ?
  =>
    let lower = source.generate(Randomness(lower_seed))?.runes().next()?
    let upper = source.generate(Randomness(upper_seed))?.runes().next()?
    h.assert_true(lower <= 0xD7FF, family + ": draw below the weighted split")
    h.assert_true(upper >= 0xE000, family + ": draw above the weighted split")

class \nodoc\ iso _GeneratorContinuationProperty is Property[U64]
  fun name(): String => "Gen/size/optional_continuation"
  fun gen(): Generator[U64] => Generators.u64()

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 1, max_shrink_reductions' = 16)

  fun property(seed: U64, h: PropertyHelper) ? =>
    _check(h, seed)?
    for witness in Range[U64].till(0, 48) do
      _check(h, witness)?
    end

  fun _check(h: PropertyHelper, witness: U64) ? =>
    let expected_sequence =
      _size(Randomness(witness), {(r) => r.usize(400, 500); None })
    _GeneratorBounds.sequences(h, witness, 0, 3, expected_sequence)?

    let expected_unique = _size(Randomness(witness), {(r: Randomness) => None })
    _GeneratorBounds.associative(h, witness, 0, 3, 3, expected_unique)?

    let expected_bytes =
      _size(Randomness(witness), {(r) => r.u8(128, 200); None })
    let bytes = Generators.byte_string(Generators.u8(128, 200), 0, 3)
      .generate(Randomness(witness))?
    h.assert_eq[USize](
      expected_bytes, bytes.size(), "byte_string seed=" + witness.string())

    let expected_scalars =
      _size(Randomness(witness), {(r) => r.u32(0x10000, 0x10FFFF); None })
    let unicode = Generators.utf32_codepoint_string(
      Generators.u32(0x10000, 0x10FFFF), 0, 3).generate(Randomness(witness))?
    h.assert_eq[USize](
      expected_scalars,
      unicode.codepoints(),
      "utf32_codepoint_string seed=" + witness.string())

  fun _size(rnd: Randomness, element: {(Randomness): None} box): USize =>
    if not rnd.biased_bool(3, 4) then return 0 end
    element(rnd)
    if not rnd.biased_bool(2, 3) then return 1 end
    element(rnd)
    if rnd.biased_bool(1, 2) then 3 else 2 end
