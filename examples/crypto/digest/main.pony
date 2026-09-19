use "crypto"

actor Main
  new create(env: Env) =>
    try
      let sha256digest: Digest = Digest.sha256()?
      sha256digest.append("Hello ")?
      sha256digest.append("World")?
      let hash: Array[U8] val = sha256digest.final()?
      env.out.print("SHA256: " + ToHexString(hash))
    else
      env.out.print("Error computing hash")
    end

    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      try
        let shake: Digest = Digest.shake256(64)?
        shake.append("Hello ")?
        shake.append("World")?
        let shake_hash: Array[U8] val = shake.final()?
        env.out.print("SHAKE256 (64 bytes): " + ToHexString(shake_hash))
      else
        env.out.print("Error computing SHAKE hash")
      end
    end
