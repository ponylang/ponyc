use "lib:crypto"

use @MD4[Pointer[U8]](d: Pointer[U8] tag, n: USize, md: Pointer[U8] tag)
use @MD5[Pointer[U8]](d: Pointer[U8] tag, n: USize, md: Pointer[U8] tag)
use @RIPEMD160[Pointer[U8]](d: Pointer[U8] tag, n: USize, md: Pointer[U8] tag)
use @SHA1[Pointer[U8]](d: Pointer[U8] tag, n: USize, md: Pointer[U8] tag)
use @SHA224[Pointer[U8]](d: Pointer[U8] tag, n: USize, md: Pointer[U8] tag)
use @SHA256[Pointer[U8]](d: Pointer[U8] tag, n: USize, md: Pointer[U8] tag)
use @SHA384[Pointer[U8]](d: Pointer[U8] tag, n: USize, md: Pointer[U8] tag)
use @SHA512[Pointer[U8]](d: Pointer[U8] tag, n: USize, md: Pointer[U8] tag)

use "format"

interface val HashFn
  """
  Produces a fixed-length byte array based on the input sequence.
  """
  fun apply(input: ByteSeq): Array[U8] val
    """
    The digest of `input`. The same input always gives the same digest, and
    every digest an implementation gives back has the same length.
    """

primitive MD4 is HashFn
  """
  Compute the MD4 message digest conforming to RFC 1320. Returns 16 bytes.

  One-shot only — `Digest` does not offer an MD4 constructor because the
  algorithm is cryptographically broken and deprecated in OpenSSL 3.x.
  """
  fun apply(input: ByteSeq): Array[U8] val =>
    recover
      let size: USize = 16
      let arr = Array[U8].init(0, size)
      @MD4(input.cpointer(), input.size(), arr.cpointer())
      arr
    end

primitive MD5 is HashFn
  """
  Compute the MD5 message digest conforming to RFC 1321. Returns 16 bytes.
  """
  fun apply(input: ByteSeq): Array[U8] val =>
    recover
      let size: USize = 16
      let arr = Array[U8].init(0, size)
      @MD5(input.cpointer(), input.size(), arr.cpointer())
      arr
    end

primitive RIPEMD160 is HashFn
  """
  Compute the RIPEMD160 message digest conforming to ISO/IEC 10118-3. Returns
  20 bytes.
  """
  fun apply(input: ByteSeq): Array[U8] val =>
    recover
      let size: USize = 20
      let arr = Array[U8].init(0, size)
      @RIPEMD160(input.cpointer(), input.size(), arr.cpointer())
      arr
    end

primitive SHA1 is HashFn
  """
  Compute the SHA1 message digest conforming to US Federal Information
  Processing Standard FIPS PUB 180-4. Returns 20 bytes.
  """
  fun apply(input: ByteSeq): Array[U8] val =>
    recover
      let size: USize = 20
      let arr = Array[U8].init(0, size)
      @SHA1(input.cpointer(), input.size(), arr.cpointer())
      arr
    end

primitive SHA224 is HashFn
  """
  Compute the SHA224 message digest conforming to US Federal Information
  Processing Standard FIPS PUB 180-4. Returns 28 bytes.
  """
  fun apply(input: ByteSeq): Array[U8] val =>
    recover
      let size: USize = 28
      let arr = Array[U8].init(0, size)
      @SHA224(input.cpointer(), input.size(), arr.cpointer())
      arr
    end

primitive SHA256 is HashFn
  """
  Compute the SHA256 message digest conforming to US Federal Information
  Processing Standard FIPS PUB 180-4. Returns 32 bytes.
  """
  fun apply(input: ByteSeq): Array[U8] val =>
    recover
      let size: USize = 32
      let arr = Array[U8].init(0, size)
      @SHA256(input.cpointer(), input.size(), arr.cpointer())
      arr
    end

primitive SHA384 is HashFn
  """
  Compute the SHA384 message digest conforming to US Federal Information
  Processing Standard FIPS PUB 180-4. Returns 48 bytes.
  """
  fun apply(input: ByteSeq): Array[U8] val =>
    recover
      let size: USize = 48
      let arr = Array[U8].init(0, size)
      @SHA384(input.cpointer(), input.size(), arr.cpointer())
      arr
    end

primitive SHA512 is HashFn
  """
  Compute the SHA512 message digest conforming to US Federal Information
  Processing Standard FIPS PUB 180-4. Returns 64 bytes.
  """
  fun apply(input: ByteSeq): Array[U8] val =>
    recover
      let size: USize = 64
      let arr = Array[U8].init(0, size)
      @SHA512(input.cpointer(), input.size(), arr.cpointer())
      arr
    end

primitive ToHexString
  """
  Return the lower-case hexadecimal string representation of the given Array
  of U8.

  ```pony
  let hex = ToHexString(SHA256("Hello World"))
  ```
  """
  fun apply(bs: Array[U8] val): String =>
    let out = recover String(bs.size() * 2) end
    for c in bs.values() do
      out.append(Format.int[U8](c where
        fmt = FormatHexSmallBare, width = 2, fill = '0'))
    end
    consume out
