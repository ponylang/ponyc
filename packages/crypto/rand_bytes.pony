use "lib:crypto"

use @RAND_bytes[I32](buf: Pointer[U8] tag, num: I32)

primitive RandBytes
  """
  Generate cryptographically secure random bytes using OpenSSL's CSPRNG.

  Returns an array of the requested number of random bytes, or raises an
  error if the CSPRNG cannot generate secure output (e.g., insufficient
  entropy during early system startup), or if `size` is larger than OpenSSL's
  `int` can hold.

  ```pony
  let nonce = RandBytes(24)?
  ```
  """
  fun apply(size: USize): Array[U8] val ? =>
    // `RAND_bytes` takes an `int`. A `size` that does not fit one narrows to a
    // smaller count, and the bytes past it stay zero while `RAND_bytes` reports
    // success. Checked before the array is allocated, so an absurd `size` costs
    // nothing.
    if size > I32.max_value().usize() then error end

    recover
      let arr = Array[U8].init(0, size)
      let rc = @RAND_bytes(arr.cpointer(), size.i32())
      if rc != 1 then error end
      arr
    end
