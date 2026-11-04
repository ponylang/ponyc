class val Seq2[A: Any val, B: Any val] is Codec[(A, B)]
  """
  Sequence two codecs, producing a tuple of their decoded values.
  """
  let _a: Codec[A]
  let _b: Codec[B]

  new val create(a: Codec[A], b: Codec[B]) =>
    _a = a
    _b = b

  fun decode(data: Array[U8] val, offset: USize)
    : (((A, B), USize) | DecodeError)
  =>
    match \exhaustive\ _a.decode(data, offset)
    | (let va: A, let ca: USize) =>
      match \exhaustive\ _b.decode(data, offset + ca)
      | (let vb: B, let cb: USize) => ((va, vb), ca + cb)
      | let e: DecodeError => e
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: (A, B), writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    match \exhaustive\ _a._encode_bits(value._1, writer)
    | let w: _BitWriter ref => _b._encode_bits(value._2, w)
    | let e: EncodeError => e
    end

class val Seq3[A: Any val, B: Any val, C: Any val]
  is Codec[(A, B, C)]
  """
  Sequence three codecs, producing a 3-tuple.
  """
  let _a: Codec[A]
  let _b: Codec[B]
  let _c: Codec[C]

  new val create(a: Codec[A], b: Codec[B], c: Codec[C]) =>
    _a = a
    _b = b
    _c = c

  fun decode(data: Array[U8] val, offset: USize)
    : (((A, B, C), USize) | DecodeError)
  =>
    match \exhaustive\ _a.decode(data, offset)
    | (let va: A, let ca: USize) =>
      match \exhaustive\ _b.decode(data, offset + ca)
      | (let vb: B, let cb: USize) =>
        match \exhaustive\ _c.decode(data, offset + ca + cb)
        | (let vc: C, let cc: USize) => ((va, vb, vc), ca + cb + cc)
        | let e: DecodeError => e
        end
      | let e: DecodeError => e
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: (A, B, C), writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    match \exhaustive\ _a._encode_bits(value._1, writer)
    | let w: _BitWriter ref =>
      match \exhaustive\ _b._encode_bits(value._2, w)
      | let w2: _BitWriter ref => _c._encode_bits(value._3, w2)
      | let e: EncodeError => e
      end
    | let e: EncodeError => e
    end

class val Seq4[A: Any val, B: Any val, C: Any val, D: Any val]
  is Codec[(A, B, C, D)]
  """
  Sequence four codecs, producing a 4-tuple.
  """
  let _a: Codec[A]
  let _b: Codec[B]
  let _c: Codec[C]
  let _d: Codec[D]

  new val create(
    a: Codec[A], b: Codec[B], c: Codec[C], d: Codec[D])
  =>
    _a = a
    _b = b
    _c = c
    _d = d

  fun decode(data: Array[U8] val, offset: USize)
    : (((A, B, C, D), USize) | DecodeError)
  =>
    match \exhaustive\ _a.decode(data, offset)
    | (let va: A, let ca: USize) =>
      match \exhaustive\ _b.decode(data, offset + ca)
      | (let vb: B, let cb: USize) =>
        match \exhaustive\ _c.decode(data, offset + ca + cb)
        | (let vc: C, let cc: USize) =>
          match \exhaustive\ _d.decode(data, offset + ca + cb + cc)
          | (let vd: D, let cd: USize) =>
            ((va, vb, vc, vd), ca + cb + cc + cd)
          | let e: DecodeError => e
          end
        | let e: DecodeError => e
        end
      | let e: DecodeError => e
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: (A, B, C, D), writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    match \exhaustive\ _a._encode_bits(value._1, writer)
    | let w: _BitWriter ref =>
      match \exhaustive\ _b._encode_bits(value._2, w)
      | let w2: _BitWriter ref =>
        match \exhaustive\ _c._encode_bits(value._3, w2)
        | let w3: _BitWriter ref => _d._encode_bits(value._4, w3)
        | let e: EncodeError => e
        end
      | let e: EncodeError => e
      end
    | let e: EncodeError => e
    end

class val Seq5[A: Any val, B: Any val, C: Any val, D: Any val, E: Any val]
  is Codec[(A, B, C, D, E)]
  """
  Sequence five codecs, producing a 5-tuple.
  """
  let _a: Codec[A]
  let _b: Codec[B]
  let _c: Codec[C]
  let _d: Codec[D]
  let _e: Codec[E]

  new val create(
    a: Codec[A], b: Codec[B], c: Codec[C], d: Codec[D], e: Codec[E])
  =>
    _a = a
    _b = b
    _c = c
    _d = d
    _e = e

  fun decode(data: Array[U8] val, offset: USize)
    : (((A, B, C, D, E), USize) | DecodeError)
  =>
    match \exhaustive\ _a.decode(data, offset)
    | (let va: A, let ca: USize) =>
      match \exhaustive\ _b.decode(data, offset + ca)
      | (let vb: B, let cb: USize) =>
        match \exhaustive\ _c.decode(data, offset + ca + cb)
        | (let vc: C, let cc: USize) =>
          match \exhaustive\ _d.decode(data, offset + ca + cb + cc)
          | (let vd: D, let cd: USize) =>
            match \exhaustive\ _e.decode(data, offset + ca + cb + cc + cd)
            | (let ve: E, let ce: USize) =>
              ((va, vb, vc, vd, ve), ca + cb + cc + cd + ce)
            | let e: DecodeError => e
            end
          | let e: DecodeError => e
          end
        | let e: DecodeError => e
        end
      | let e: DecodeError => e
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: (A, B, C, D, E), writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    match \exhaustive\ _a._encode_bits(value._1, writer)
    | let w: _BitWriter ref =>
      match \exhaustive\ _b._encode_bits(value._2, w)
      | let w2: _BitWriter ref =>
        match \exhaustive\ _c._encode_bits(value._3, w2)
        | let w3: _BitWriter ref =>
          match \exhaustive\ _d._encode_bits(value._4, w3)
          | let w4: _BitWriter ref => _e._encode_bits(value._5, w4)
          | let e: EncodeError => e
          end
        | let e: EncodeError => e
        end
      | let e: EncodeError => e
      end
    | let e: EncodeError => e
    end

class val Seq6[
  A: Any val,
  B: Any val,
  C: Any val,
  D: Any val,
  E: Any val,
  F: Any val]
  is Codec[(A, B, C, D, E, F)]
  """
  Sequence six codecs, producing a 6-tuple. Larger structures compose from
  smaller Seq groups via `Mapped`.
  """
  let _a: Codec[A]
  let _b: Codec[B]
  let _c: Codec[C]
  let _d: Codec[D]
  let _e: Codec[E]
  let _f: Codec[F]

  new val create(
    a: Codec[A],
    b: Codec[B],
    c: Codec[C],
    d: Codec[D],
    e: Codec[E],
    f: Codec[F])
  =>
    _a = a
    _b = b
    _c = c
    _d = d
    _e = e
    _f = f

  fun decode(data: Array[U8] val, offset: USize)
    : (((A, B, C, D, E, F), USize) | DecodeError)
  =>
    match \exhaustive\ _a.decode(data, offset)
    | (let va: A, let ca: USize) =>
      match \exhaustive\ _b.decode(data, offset + ca)
      | (let vb: B, let cb: USize) =>
        match \exhaustive\ _c.decode(data, offset + ca + cb)
        | (let vc: C, let cc: USize) =>
          match \exhaustive\ _d.decode(data, offset + ca + cb + cc)
          | (let vd: D, let cd: USize) =>
            match \exhaustive\ _e.decode(data, offset + ca + cb + cc + cd)
            | (let ve: E, let ce: USize) =>
              let of' = offset + ca + cb + cc + cd + ce
              match \exhaustive\ _f.decode(data, of')
              | (let vf: F, let cf: USize) =>
                ((va, vb, vc, vd, ve, vf), ca + cb + cc + cd + ce + cf)
              | let e: DecodeError => e
              end
            | let e: DecodeError => e
            end
          | let e: DecodeError => e
          end
        | let e: DecodeError => e
        end
      | let e: DecodeError => e
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: (A, B, C, D, E, F), writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    match \exhaustive\ _a._encode_bits(value._1, writer)
    | let w: _BitWriter ref =>
      match \exhaustive\ _b._encode_bits(value._2, w)
      | let w2: _BitWriter ref =>
        match \exhaustive\ _c._encode_bits(value._3, w2)
        | let w3: _BitWriter ref =>
          match \exhaustive\ _d._encode_bits(value._4, w3)
          | let w4: _BitWriter ref =>
            match \exhaustive\ _e._encode_bits(value._5, w4)
            | let w5: _BitWriter ref =>
              _f._encode_bits(value._6, w5)
            | let e: EncodeError => e
            end
          | let e: EncodeError => e
          end
        | let e: EncodeError => e
        end
      | let e: EncodeError => e
      end
    | let e: EncodeError => e
    end
