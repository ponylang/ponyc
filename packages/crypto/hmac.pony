use "path:/usr/local/opt/libressl/lib" if osx
use "lib:crypto" if not windows
use "lib:libcrypto-32" if windows

primitive HMAC
  """
  Produces the authentication code for the input byte sequence using given key.
  First argument accepts a name of digest algorithm. Currently supported algorithms are:

    * ``MD5``
    * ``RIPEMD160``
    * ``SHA1``
    * ``SHA224``
    * ``SHA256``
    * ``SHA384``
    * ``SHA512``

  ```pony
  use "crypto"

  actor Main
    new create(env: Env) =>
      env.out.print(ToHexString(HMAC("SHA1", "secret", "Hello, world")))
      // => c671e00ea137e7a4bea30ae0d4969e7408a0f0f2
  ```
  """
  fun tag apply(digest: String, key: ByteSeq, input: ByteSeq): Array[U8] val ? =>
    recover
      (let md, var size: U32) =
        match(digest)
          | "MD5" => (@EVP_md5[Pointer[_EVPMD]](), 16)
          | "RIPEMD160" => (@EVP_ripemd160[Pointer[_EVPMD]](), 20)
          | "SHA1" => (@EVP_sha1[Pointer[_EVPMD]](), 20)
          | "SHA224" => (@EVP_sha224[Pointer[_EVPMD]](), 28)
          | "SHA256" => (@EVP_sha256[Pointer[_EVPMD]](), 32)
          | "SHA384" => (@EVP_sha384[Pointer[_EVPMD]](), 48)
          | "SHA512" => (@EVP_sha512[Pointer[_EVPMD]](), 64)
        else
          error
        end
      let hash = @pony_alloc[Pointer[U8]](
        @pony_ctx[Pointer[None] iso](),
        size.usize()
      )
      @HMAC[Pointer[U8]](md, key.cpointer(), key.size(),
        input.cpointer(), input.size(), hash, addressof size)
      Array[U8].from_cpointer(hash, size.usize())
    end
