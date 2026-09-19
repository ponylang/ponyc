use "crypto"

actor Main
  new create(env: Env) =>
    let md5hash: Array[U8] val = MD5("Hello World")
    env.out.print("MD5: " + ToHexString(md5hash))

    let sha1hash: Array[U8] val = SHA1("Hello World")
    env.out.print("SHA1: " + ToHexString(sha1hash))

    let sha256hash: Array[U8] val = SHA256("Hello World")
    env.out.print("SHA256: " + ToHexString(sha256hash))
