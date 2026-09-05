primitive Sha256
  """
  SHA-256 cryptographic hash (FIPS 180-4). Computes a 32-byte digest
  from an arbitrary byte sequence.
  """
  fun apply(data: ReadSeq[U8] val): Array[U8] val =>
    """
    Returns the 32-byte SHA-256 digest of `data`.
    """
    var h0: U32 = 0x6a09e667
    var h1: U32 = 0xbb67ae85
    var h2: U32 = 0x3c6ef372
    var h3: U32 = 0xa54ff53a
    var h4: U32 = 0x510e527f
    var h5: U32 = 0x9b05688c
    var h6: U32 = 0x1f83d9ab
    var h7: U32 = 0x5be0cd19

    let msg_len = data.size()
    let bit_len: U64 = msg_len.u64() * 8

    // FIPS 180-4 Section 5.1.1
    let padded_len = ((msg_len + 9) + 63) and not USize(63)
    let padded: Array[U8] val =
      recover val
        let p = Array[U8](padded_len)
        var i: USize = 0
        while i < msg_len do
          try p.push(data(i)?) else _Unreachable() end
          i = i + 1
        end
        p.push(0x80)
        var z: USize = padded_len - msg_len - 9
        while z > 0 do
          p.push(0)
          z = z - 1
        end
        p.push((bit_len >> 56).u8())
        p.push((bit_len >> 48).u8())
        p.push((bit_len >> 40).u8())
        p.push((bit_len >> 32).u8())
        p.push((bit_len >> 24).u8())
        p.push((bit_len >> 16).u8())
        p.push((bit_len >> 8).u8())
        p.push(bit_len.u8())
        p
      end

    let k = _round_constants()
    let w = Array[U32](64)

    var block: USize = 0
    while block < padded_len do
      w.clear()
      var t: USize = 0
      while t < 16 do
        let i = block + (t * 4)
        try
          w.push(
            (padded(i)?.u32() << 24) or
              (padded(i + 1)?.u32() << 16) or
              (padded(i + 2)?.u32() << 8) or
              padded(i + 3)?.u32())
        else
          _Unreachable()
        end
        t = t + 1
      end
      while t < 64 do
        try
          w.push(
            _lsigma1(w(t - 2)?) + w(t - 7)? +
              _lsigma0(w(t - 15)?) + w(t - 16)?)
        else
          _Unreachable()
        end
        t = t + 1
      end

      var a: U32 = h0; var b: U32 = h1
      var c: U32 = h2; var d: U32 = h3
      var e: U32 = h4; var f: U32 = h5
      var g: U32 = h6; var h: U32 = h7

      t = 0
      while t < 64 do
        try
          let t1 = h + _usigma1(e) + _ch(e, f, g) + k(t)? + w(t)?
          let t2 = _usigma0(a) + _maj(a, b, c)
          h = g; g = f; f = e; e = d + t1
          d = c; c = b; b = a; a = t1 + t2
        else
          _Unreachable()
        end
        t = t + 1
      end

      h0 = h0 + a; h1 = h1 + b; h2 = h2 + c; h3 = h3 + d
      h4 = h4 + e; h5 = h5 + f; h6 = h6 + g; h7 = h7 + h

      block = block + 64
    end

    recover val
      let out = Array[U8](32)
      _be32(out, h0); _be32(out, h1); _be32(out, h2); _be32(out, h3)
      _be32(out, h4); _be32(out, h5); _be32(out, h6); _be32(out, h7)
      out
    end

  fun hex(digest: ReadSeq[U8] val): String val =>
    """
    Hex-encodes a byte sequence as lowercase hexadecimal.
    """
    let hex_chars: String val = "0123456789abcdef"
    recover val
      let s = String(digest.size() * 2)
      var i: USize = 0
      while i < digest.size() do
        try
          let byte = digest(i)?
          s.push(hex_chars((byte >> 4).usize())?)
          s.push(hex_chars((byte and 0x0f).usize())?)
        else
          _Unreachable()
        end
        i = i + 1
      end
      s
    end

  fun _ch(x: U32, y: U32, z: U32): U32 =>
    (x and y) xor ((not x) and z)

  fun _maj(x: U32, y: U32, z: U32): U32 =>
    (x and y) xor (x and z) xor (y and z)

  fun _usigma0(x: U32): U32 =>
    x.rotr(2) xor x.rotr(13) xor x.rotr(22)

  fun _usigma1(x: U32): U32 =>
    x.rotr(6) xor x.rotr(11) xor x.rotr(25)

  fun _lsigma0(x: U32): U32 =>
    x.rotr(7) xor x.rotr(18) xor (x >> 3)

  fun _lsigma1(x: U32): U32 =>
    x.rotr(17) xor x.rotr(19) xor (x >> 10)

  fun _be32(out: Array[U8], v: U32) =>
    out.push((v >> 24).u8())
    out.push((v >> 16).u8())
    out.push((v >> 8).u8())
    out.push(v.u8())

  fun _round_constants(): Array[U32] val =>
    // FIPS 180-4 Section 4.2.2
    [ as U32:
      0x428a2f98; 0x71374491; 0xb5c0fbcf; 0xe9b5dba5
      0x3956c25b; 0x59f111f1; 0x923f82a4; 0xab1c5ed5
      0xd807aa98; 0x12835b01; 0x243185be; 0x550c7dc3
      0x72be5d74; 0x80deb1fe; 0x9bdc06a7; 0xc19bf174
      0xe49b69c1; 0xefbe4786; 0x0fc19dc6; 0x240ca1cc
      0x2de92c6f; 0x4a7484aa; 0x5cb0a9dc; 0x76f988da
      0x983e5152; 0xa831c66d; 0xb00327c8; 0xbf597fc7
      0xc6e00bf3; 0xd5a79147; 0x06ca6351; 0x14292967
      0x27b70a85; 0x2e1b2138; 0x4d2c6dfc; 0x53380d13
      0x650a7354; 0x766a0abb; 0x81c2c92e; 0x92722c85
      0xa2bfe8a1; 0xa81a664b; 0xc24b8b70; 0xc76c51a3
      0xd192e819; 0xd6990624; 0xf40e3585; 0x106aa070
      0x19a4c116; 0x1e376c08; 0x2748774c; 0x34b0bcb5
      0x391c0cb3; 0x4ed8aa4a; 0x5b9cca4f; 0x682e6ff3
      0x748f82ee; 0x78a5636f; 0x84c87814; 0x8cc70208
      0x90befffa; 0xa4506ceb; 0xbef9a3f7; 0xc67178f2
    ]
