use "lib:crypto"

use @PKCS5_PBKDF2_HMAC[I32](
  pass: Pointer[U8] tag, passlen: I32,
  salt: Pointer[U8] tag, saltlen: I32, iter: I32,
  digest: Pointer[_EVPMD],
  keylen: I32, out: Pointer[U8] tag)
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"

primitive Pbkdf2Sha256
  """
  Derive a key from a password using PBKDF2 with HMAC-SHA-256 as the PRF,
  as defined in RFC 2898.

  Returns a key of the requested length, or raises an error if the derivation
  fails (e.g., zero iterations), or if any of the iteration count, the key
  length, or the password or salt length is larger than OpenSSL's `int` can
  hold.

  ```pony
  let key = Pbkdf2Sha256("password", "salt", 4096, 32)?
  ```
  """
  fun apply(
    password: ByteSeq,
    salt: ByteSeq,
    iterations: U32,
    key_length: USize)
    : Array[U8] val ?
  =>
    // `PKCS5_PBKDF2_HMAC` takes an `int` for the iteration count and for each
    // length. A value that does not fit one narrows, and the backends do not
    // agree on what they do with what arrives. Checked before the key array is
    // allocated.
    if
      (iterations == 0) or
        (password.size() > I32.max_value().usize()) or
        (salt.size() > I32.max_value().usize()) or
        (key_length > I32.max_value().usize()) or
        (iterations > I32.max_value().u32())
    then
      error
    end

    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      recover
        let arr = Array[U8].init(0, key_length)
        let rc =
          @PKCS5_PBKDF2_HMAC(
            password.cpointer(),
            password.size().i32(),
            salt.cpointer(),
            salt.size().i32(),
            iterations.i32(),
            @EVP_sha256(),
            key_length.i32(),
            arr.cpointer())
        if rc != 1 then error end
        arr
      end
    else
      compile_error "You must select an SSL version to use."
    end
