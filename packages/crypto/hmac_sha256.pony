use "lib:crypto"

use @HMAC[Pointer[U8]](
  evp_md: Pointer[_EVPMD],
  key: Pointer[U8] tag, key_len: I32,
  data: Pointer[U8] tag, data_len: USize,
  md: Pointer[U8] tag, md_len: Pointer[U32])

primitive HmacSha256
  """
  Compute HMAC using SHA-256 as the hash function, as defined in RFC 2104.

  Returns a 32-byte message authentication code.

  ```pony
  let mac = HmacSha256("secret-key", "Hello, World!")?
  ```

  Raises an error when the key is longer than OpenSSL's `int` can hold, and when
  OpenSSL could not compute the code.

  When it raises, reject. Do not compare a message against a code of your own
  making: a code you invent is one an attacker can send you.
  """
  fun apply(key: ByteSeq, data: ByteSeq): Array[U8] val ? =>
    if key.size() > I32.max_value().usize() then error end

    recover
      let size: USize = 32
      let arr = Array[U8].init(0, size)

      // `HMAC` returns NULL when the message pointer is null, even when the
      // message is empty, and an empty `Array[U8]` has a null pointer. A code
      // over an empty message is well defined, so hand it a pointer to bytes it
      // will not read.
      let message =
        if data.size() == 0 then arr.cpointer() else data.cpointer() end

      // `HMAC` accepts a null key pointer when `key_len` is 0, unlike the
      // message pointer which causes a NULL return.
      let md =
        @HMAC(
          @EVP_sha256(),
          key.cpointer(),
          key.size().i32(),
          message,
          data.size(),
          arr.cpointer(),
          Pointer[U32])

      // `HMAC` writes nothing when it fails, and `arr` is 32 zero bytes, which
      // is a code an attacker can send. Raise rather than return it.
      if md.is_null() then error end
      arr
    end
