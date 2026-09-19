use @CRYPTO_memcmp[I32](a: Pointer[U8] tag, b: Pointer[U8] tag, len: USize)

primitive ConstantTimeCompare
  """
  Return true if the two `ByteSeq`s have equal contents. The comparison runs
  in time independent of the contents, so comparing a secret against a value an
  attacker supplies does not tell them where the two first differ.

  ```pony
  if ConstantTimeCompare(expected_mac, supplied_mac) then
    // ...
  end
  ```

  Sequences of different sizes are not equal, and that is answered before any
  byte is read.
  """
  fun apply[S: ByteSeq box = ByteSeq box](xs: S, ys: S): Bool =>
    if xs.size() != ys.size() then
      false
    else
      @CRYPTO_memcmp(xs.cpointer(), ys.cpointer(), xs.size()) == 0
    end
