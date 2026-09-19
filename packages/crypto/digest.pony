use "lib:crypto"
use "lib:bcrypt" if windows

use @EVP_MD_CTX_new[Pointer[_EVPCTX]]()
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
use @EVP_DigestInit_ex[I32](
  ctx: Pointer[_EVPCTX] tag,
  t: Pointer[_EVPMD],
  impl: Pointer[None])
use @EVP_DigestUpdate[I32](
  ctx: Pointer[_EVPCTX] tag,
  d: Pointer[U8] tag,
  cnt: USize)
use @EVP_DigestFinal_ex[I32](
  ctx: Pointer[_EVPCTX] tag,
  md: Pointer[U8] tag,
  s: Pointer[U32])
use @EVP_DigestFinalXOF[I32](
  ctx: Pointer[_EVPCTX] tag,
  md: Pointer[U8] tag,
  len: USize)
  if "openssl_3.0.x" or "openssl_4.0.x"
use @EVP_MD_CTX_free[None](ctx: Pointer[_EVPCTX] tag)
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"

use @EVP_md5[Pointer[_EVPMD]]()
use @EVP_ripemd160[Pointer[_EVPMD]]()
use @EVP_sha1[Pointer[_EVPMD]]()
use @EVP_sha224[Pointer[_EVPMD]]()
use @EVP_sha256[Pointer[_EVPMD]]()
use @EVP_sha384[Pointer[_EVPMD]]()
use @EVP_sha512[Pointer[_EVPMD]]()
use @EVP_shake128[Pointer[_EVPMD]]()
use @EVP_shake256[Pointer[_EVPMD]]()

primitive _EVPMD
primitive _EVPCTX

primitive _EVPContext
  fun apply(md: Pointer[_EVPMD]): Pointer[_EVPCTX] ? =>
    """
    A context initialised for `md`. Raises when OpenSSL could not give us one.
    """
    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      let ctx = @EVP_MD_CTX_new()
      if ctx.is_null() then error end

      if @EVP_DigestInit_ex(ctx, md, Pointer[None]) != 1 then
        @EVP_MD_CTX_free(ctx)
        error
      end

      ctx
    else
      compile_error "You must select an SSL version to use."
    end

class Digest
  """
  Produces a hash from the chunks of input. Feed the input with `append()` and
  produce a final hash from the concatenation of the input with `final()`.

  `append()` accumulates input; `final()` returns the hash of the
  concatenation, or raises. It never returns a hash of anything other than what
  was appended. Construction raises when OpenSSL fails to allocate a context;
  `append()` and `final()` raise when OpenSSL reports failure.
  """
  let _digest_size: USize
  var _ctx: Pointer[_EVPCTX] = Pointer[_EVPCTX]
  let _variable_length: Bool
  var _hash: (Array[U8] val | None) = None

  new md5() ? =>
    """
    16-byte (128-bit) hash.
    """
    _variable_length = false
    _digest_size = 16
    _ctx = _EVPContext(@EVP_md5())?

  new ripemd160() ? =>
    """
    20-byte (160-bit) hash.
    """
    _variable_length = false
    _digest_size = 20
    _ctx = _EVPContext(@EVP_ripemd160())?

  new sha1() ? =>
    """
    20-byte (160-bit) hash.
    """
    _variable_length = false
    _digest_size = 20
    _ctx = _EVPContext(@EVP_sha1())?

  new sha224() ? =>
    """
    28-byte (224-bit) hash.
    """
    _variable_length = false
    _digest_size = 28
    _ctx = _EVPContext(@EVP_sha224())?

  new sha256() ? =>
    """
    32-byte (256-bit) hash.
    """
    _variable_length = false
    _digest_size = 32
    _ctx = _EVPContext(@EVP_sha256())?

  new sha384() ? =>
    """
    48-byte (384-bit) hash.
    """
    _variable_length = false
    _digest_size = 48
    _ctx = _EVPContext(@EVP_sha384())?

  new sha512() ? =>
    """
    64-byte (512-bit) hash.
    """
    _variable_length = false
    _digest_size = 64
    _ctx = _EVPContext(@EVP_sha512())?

  new shake128(size': USize = 16) ? =>
    """
    SHAKE128 is an extendable output function (XOF) that can produce
    variable-length output. The `size'` parameter controls the output length
    in bytes (default: 16). Variable-length output requires OpenSSL 3.0.x or
    OpenSSL 4.0.x; on OpenSSL 1.1.x, only the default size is accepted.
    """
    ifdef "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" then
      ifdef "openssl_3.0.x" or "openssl_4.0.x" then
        _variable_length = true
        _digest_size = size'
      else
        if size' != 16 then error end
        _variable_length = false
        _digest_size = 16
      end
      _ctx = _EVPContext(@EVP_shake128())?
    else
      compile_error "shake128 needs OpenSSL 1.1.x, 3.0.x or 4.0.x"
    end

  new shake256(size': USize = 32) ? =>
    """
    SHAKE256 is an extendable output function (XOF) that can produce
    variable-length output. The `size'` parameter controls the output length
    in bytes (default: 32). Variable-length output requires OpenSSL 3.0.x or
    OpenSSL 4.0.x; on OpenSSL 1.1.x, only the default size is accepted.
    """
    ifdef "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" then
      ifdef "openssl_3.0.x" or "openssl_4.0.x" then
        _variable_length = true
        _digest_size = size'
      else
        if size' != 32 then error end
        _variable_length = false
        _digest_size = 32
      end
      _ctx = _EVPContext(@EVP_shake256())?
    else
      compile_error "shake256 needs OpenSSL 1.1.x, 3.0.x or 4.0.x"
    end

  fun ref append(input: ByteSeq) ? =>
    """
    Update the digest with input.

    Raises an error when `final()` has already been called, and when OpenSSL
    could not take the input.
    """
    if _ctx.is_null() then error end
    if @EVP_DigestUpdate(_ctx, input.cpointer(), input.size()) != 1 then
      error
    end

  fun ref final(): Array[U8] val ? =>
    """
    Return the hash of the input passed to `append()`. A second call returns
    the hash the first one produced.

    Raises an error when OpenSSL could not produce the hash. A digest that
    raises here has no hash to give, and raises from every later call.
    """
    match _hash
    | let h: Array[U8] val => h
    else
      if _ctx.is_null() then error end

      let size = _digest_size
      let digest = recover Array[U8].init(0, size) end

      var rc: I32 = 0
      ifdef "openssl_3.0.x" or "openssl_4.0.x" then
        rc =
          if _variable_length then
            @EVP_DigestFinalXOF(_ctx, digest.cpointer(), size)
          else
            @EVP_DigestFinal_ex(_ctx, digest.cpointer(), Pointer[U32])
          end
      elseif "openssl_1.1.x" or "libressl" then
        rc = @EVP_DigestFinal_ex(_ctx, digest.cpointer(), Pointer[U32])
      else
        compile_error "You must select an SSL version to use."
      end

      ifdef
        "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
      then
        @EVP_MD_CTX_free(_ctx)
      else
        compile_error "You must select an SSL version to use."
      end
      _ctx = Pointer[_EVPCTX]

      // On failure OpenSSL wrote nothing, so `digest` is still all zeros.
      // Raise rather than hand a buffer OpenSSL did not fill back as a hash.
      if rc != 1 then error end

      let h: Array[U8] val = consume digest
      _hash = h
      h
    end

  fun _final() =>
    """
    Free the context of a digest that was dropped without a call to `final()`.
    """
    if not _ctx.is_null() then
      ifdef
        "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
      then
        @EVP_MD_CTX_free(_ctx)
      else
        compile_error "You must select an SSL version to use."
      end
    end

  fun digest_size(): USize =>
    """
    Return the size of the message digest in bytes.
    """
    _digest_size
