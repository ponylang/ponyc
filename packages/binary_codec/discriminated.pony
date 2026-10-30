class val _DiscriminatedCase[
  D: (Equatable[D] val & Stringable val),
  Msg: Any val]
  let disc: D
  let codec: Codec[Msg]
  let test_fn: {(Msg): Bool} val

  new val create(
    d: D,
    c: Codec[Msg],
    t: {(Msg): Bool} val)
  =>
    disc = d
    codec = c
    test_fn = t

class val Discriminated[
  D: (Equatable[D] val & Stringable val),
  Msg: Any val]
  is Codec[Msg]
  """
  Tagged union dispatch: a discriminator codec selects which variant codec
  to use.

  On decode, reads the discriminator and dispatches to the matching variant
  codec. On encode, tests each variant's narrowing function to find a match,
  then writes the discriminator followed by the variant body.

  Built via `DiscriminatedBuilder` — not constructed directly.
  """
  let _disc_codec: Codec[D]
  let _cases: Array[_DiscriminatedCase[D, Msg]] val

  new val _create(
    dc: Codec[D],
    cs: Array[_DiscriminatedCase[D, Msg]] val)
  =>
    _disc_codec = dc
    _cases = cs

  fun decode(data: Array[U8] val, offset: USize)
    : ((Msg, USize) | DecodeError)
  =>
    match \exhaustive\ _disc_codec.decode(data, offset)
    | (let d: D, let cd: USize) =>
      var i: USize = 0
      while i < _cases.size() do
        try
          let c = _cases(i)?
          if c.disc == d then
            match c.codec.decode(data, offset + cd)
            | (let v: Msg, let cv: USize) => return (v, cd + cv)
            | let e: DecodeError => return e
            end
          end
        end
        i = i + 1
      end
      MalformedData("Unknown discriminator: " + d.string(), offset)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: Msg, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    var i: USize = 0
    while i < _cases.size() do
      try
        let c = _cases(i)?
        if c.test_fn(value) then
          match _disc_codec._encode_bits(c.disc, writer)
          | let w: _BitWriter ref => return c.codec._encode_bits(value, w)
          | let e: EncodeError => return e
          end
        end
      end
      i = i + 1
    end
    EncodeViolation("No matching variant for encode")

class ref DiscriminatedBuilder[
  D: (Equatable[D] val & Stringable val),
  Msg: Any val]
  """
  Builder for constructing a `Discriminated` codec.

  Register each variant with `case_of`, providing the discriminator value,
  a codec for the variant type, and a narrowing function that casts from
  `Msg` to the variant. Duplicate discriminators are ignored (first
  registration wins). Call `build()` to produce the `Discriminated` codec.
  """
  let _disc_codec: Codec[D]
  let _cases: Array[_DiscriminatedCase[D, Msg]] ref

  new ref create(disc_codec: Codec[D]) =>
    _disc_codec = disc_codec
    _cases = Array[_DiscriminatedCase[D, Msg]]

  fun ref case_of[V: Msg](
    disc: D,
    codec: Codec[V],
    narrow: {(Msg): V ?} val)
    : DiscriminatedBuilder[D, Msg] ref
  =>
    """
    Register a variant with its discriminator, codec, and narrowing function.
    """
    for c in _cases.values() do
      if c.disc == disc then return this end
    end
    let widened = Widen[V, Msg](codec, narrow)
    let test_fn: {(Msg): Bool} val =
      {(m: Msg): Bool => try narrow(m)?; true else false end }
    _cases.push(_DiscriminatedCase[D, Msg](disc, widened, test_fn))
    this

  fun ref build(): Discriminated[D, Msg] =>
    let cs: Array[_DiscriminatedCase[D, Msg]] val =
      _cases.clone()
    Discriminated[D, Msg]._create(_disc_codec, cs)
