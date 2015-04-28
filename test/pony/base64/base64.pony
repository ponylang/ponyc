use "encode/base64"

actor Main
  new create(env: Env) =>
    let src1 =
      """
      Man is distinguished, not only by his reason, but by this singular
      passion from other animals, which is a lust of the mind, that by a
      perseverance of delight in the continued and indefatigable generation of
      knowledge, exceeds the short vehemence of any carnal pleasure.
      """

    let src2 = "Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, exceeds the short vehemence of any carnal pleasure."

    let dst =
    """
    TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz
    IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg
    dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu
    dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo
    ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=
    """

    let enc1 = recover val Base64.encode_mime(src1) end
    let enc2 = recover val Base64.encode(src2, '+', '/', '=', 76, "\n") end

    env.out.print((enc1 == enc2).string())

    env.out.print(src1)
    env.out.print(src2)
    env.out.print(enc1)

    env.out.print(enc2)
    env.out.print(dst)
    env.out.print((dst == enc2).string())

    try
      let dec1 = recover val Base64.decode(enc1) end
      let dec2 = recover val Base64.decode(enc2) end
      env.out.print(dec1)
      env.out.print(dec2)
    end

    try
      let src3 = "any carnal pleasure"
      let enc3 = recover val Base64.encode(src3) end
      let dec3 = recover val Base64.decode(enc3) end
      env.out.print((src3.size() == dec3.size()).string())
    end
