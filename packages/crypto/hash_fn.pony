use "path:/usr/local/opt/libressl/lib" if osx
use "lib:crypto" if not windows
use "lib:libcrypto-32" if windows

use "format"

interface HashFn
  """
  Produces a fixed-length byte array based on the input sequence.
  """
  fun tag apply(input: ByteSeq): Array[U8] val

primitive MD4 is HashFn
  fun tag apply(input: ByteSeq): Array[U8] val =>
    """
    Compute the MD4 message digest conforming to RFC 1320
    """
    recover
      let size: USize = 16
      let digest = @pony_alloc[Pointer[U8]](
        @pony_ctx[Pointer[None] iso](),
        size
      )
      @MD4[Pointer[U8]](input.cstring(), input.size(), digest)
      Array[U8].from_cstring(digest, size)
    end

primitive MD5 is HashFn
  fun tag apply(input: ByteSeq): Array[U8] val =>
    """
    Compute the MD5 message digest conforming to RFC 1321
    """
    recover
      let size: USize = 16
      let digest = @pony_alloc[Pointer[U8]](
        @pony_ctx[Pointer[None] iso](),
        size
      )
      @MD5[Pointer[U8]](input.cstring(), input.size(), digest)
      Array[U8].from_cstring(digest, size)
    end

primitive RIPEMD160 is HashFn
  fun tag apply(input: ByteSeq): Array[U8] val =>
    """
    Compute the RIPEMD160 message digest conforming to ISO/IEC 10118-3
    """
    recover
      let size: USize = 20
      let digest = @pony_alloc[Pointer[U8]](
        @pony_ctx[Pointer[None] iso](),
        size
      )
      @RIPEMD160[Pointer[U8]](input.cstring(), input.size(), digest)
      Array[U8].from_cstring(digest, size)
    end

primitive SHA1 is HashFn
  fun tag apply(input: ByteSeq): Array[U8] val =>
    """
    Compute the SHA1 message digest conforming to US Federal Information
    Processing Standard FIPS PUB 180-4
    """
    recover
      let size: USize = 20
      let digest = @pony_alloc[Pointer[U8]](
        @pony_ctx[Pointer[None] iso](),
        size
      )
      @SHA1[Pointer[U8]](input.cstring(), input.size(), digest)
      Array[U8].from_cstring(digest, size)
    end

primitive SHA224 is HashFn
  fun tag apply(input: ByteSeq): Array[U8] val =>
    """
    Compute the SHA224 message digest conforming to US Federal Information
    Processing Standard FIPS PUB 180-4
    """
    recover
      let size: USize = 28
      let digest = @pony_alloc[Pointer[U8]](
        @pony_ctx[Pointer[None] iso](),
        size
      )
      @SHA224[Pointer[U8]](input.cstring(), input.size(), digest)
      Array[U8].from_cstring(digest, size)
    end

primitive SHA256 is HashFn
  fun tag apply(input: ByteSeq): Array[U8] val =>
    """
    Compute the SHA256 message digest conforming to US Federal Information
    Processing Standard FIPS PUB 180-4
    """
    recover
      let size: USize = 32
      let digest = @pony_alloc[Pointer[U8]](
        @pony_ctx[Pointer[None] iso](),
        size
      )
      @SHA256[Pointer[U8]](input.cstring(), input.size(), digest)
      Array[U8].from_cstring(digest, size)
    end

primitive SHA384 is HashFn
  fun tag apply(input: ByteSeq): Array[U8] val =>
    """
    Compute the SHA384 message digest conforming to US Federal Information
    Processing Standard FIPS PUB 180-4
    """
    recover
      let size: USize = 48
      let digest = @pony_alloc[Pointer[U8]](
        @pony_ctx[Pointer[None] iso](),
        size
      )
      @SHA384[Pointer[U8]](input.cstring(), input.size(), digest)
      Array[U8].from_cstring(digest, size)
    end

primitive SHA512 is HashFn
  fun tag apply(input: ByteSeq): Array[U8] val =>
    """
    Compute the SHA512 message digest conforming to US Federal Information
    Processing Standard FIPS PUB 180-4
    """
    recover
      let size: USize = 64
      let digest = @pony_alloc[Pointer[U8]](
        @pony_ctx[Pointer[None] iso](),
        size
      )
      @SHA512[Pointer[U8]](input.cstring(), input.size(), digest)
      Array[U8].from_cstring(digest, size)
    end

primitive ToHexString
  fun tag apply(bs: Array[U8] val): String =>
    """
    Return the lower-case hexadecimal string representation of the given Array
    of U8.
    """
    let out = recover String(bs.size() * 2) end
    for c in bs.values() do
      out.append(Format.int[U8](c where
        fmt=FormatHexSmallBare, width=2, fill='0'))
    end
    consume out
