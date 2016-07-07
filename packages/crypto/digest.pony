use "path:/usr/local/opt/libressl/lib" if osx
use "lib:crypto" if not windows
use "lib:libcrypto-32" if windows

primitive _EVPMD
primitive _EVPCTX

class Digest
  """
  Produces a hash from the chunks of input. Feed the input with append() and
  produce a final hash from the concatenation of the input with final().
  """
  let _digest_size: USize
  let _ctx: Pointer[_EVPCTX]
  var _hash: (Array[U8] val | None) = None

  new md5() =>
    """
    Use the MD5 algorithm to calculate the hash.
    """
    _digest_size = 16
    _ctx = @EVP_MD_CTX_create[Pointer[_EVPCTX]]()
    @EVP_DigestInit_ex[None](_ctx, @EVP_md5[Pointer[_EVPMD]](), USize(0))

  new ripemd160() =>
    """
    Use the RIPEMD160 algorithm to calculate the hash.
    """
    _digest_size = 20
    _ctx = @EVP_MD_CTX_create[Pointer[_EVPCTX]]()
    @EVP_DigestInit_ex[None](_ctx, @EVP_ripemd160[Pointer[_EVPMD]](), USize(0))

  new sha1() =>
    """
    Use the SHA1 algorithm to calculate the hash.
    """
    _digest_size = 20
    _ctx = @EVP_MD_CTX_create[Pointer[_EVPCTX]]()
    @EVP_DigestInit_ex[None](_ctx, @EVP_sha1[Pointer[_EVPMD]](), USize(0))

  new sha224() =>
    """
    Use the SHA256 algorithm to calculate the hash.
    """
    _digest_size = 28
    _ctx = @EVP_MD_CTX_create[Pointer[_EVPCTX]]()
    @EVP_DigestInit_ex[None](_ctx, @EVP_sha224[Pointer[_EVPMD]](), USize(0))

  new sha256() =>
    """
    Use the SHA256 algorithm to calculate the hash.
    """
    _digest_size = 32
    _ctx = @EVP_MD_CTX_create[Pointer[_EVPCTX]]()
    @EVP_DigestInit_ex[None](_ctx, @EVP_sha256[Pointer[_EVPMD]](), USize(0))

  new sha384() =>
    """
    Use the SHA384 algorithm to calculate the hash.
    """
    _digest_size = 48
    _ctx = @EVP_MD_CTX_create[Pointer[_EVPCTX]]()
    @EVP_DigestInit_ex[None](_ctx, @EVP_sha384[Pointer[_EVPMD]](), USize(0))

  new sha512() =>
    """
    Use the SHA512 algorithm to calculate the hash.
    """
    _digest_size = 64
    _ctx = @EVP_MD_CTX_create[Pointer[_EVPCTX]]()
    @EVP_DigestInit_ex[None](_ctx, @EVP_sha512[Pointer[_EVPMD]](), USize(0))

  fun ref append(input: ByteSeq) ? =>
    """
    Update the Digest object with input. Throw an error if final() has been
    called.
    """
    if _hash isnt None then error end
    @EVP_DigestUpdate[None](_ctx, input.cstring(), input.size())

  fun ref final(): Array[U8] val =>
    """
    Return the digest of the strings passed to the append() method.
    """
    match _hash
    | let h: Array[U8] val => h
    else
      let size = _digest_size
      let digest = recover String.from_cstring(
        @pony_alloc[Pointer[U8]](@pony_ctx[Pointer[None] iso](), size), size
      ) end
      @EVP_DigestFinal_ex[None](_ctx, digest.cstring(), Pointer[USize])
      @EVP_MD_CTX_cleanup[None](_ctx)
      let h = (consume digest).array()
      _hash = h
      h
    end

  fun digest_size(): USize =>
    """
    Return the size of the message digest in bytes.
    """
    _digest_size
