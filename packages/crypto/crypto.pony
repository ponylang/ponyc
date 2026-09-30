"""
Cryptographic primitives built on OpenSSL's libcrypto.

For a single input, use the one-shot hash functions (`MD5`, `SHA256`, etc.) —
they take a `ByteSeq` and return `Array[U8] val` in one call. For input that
arrives in pieces, use `Digest`: construct one for the algorithm you want, feed
it with `append()`, and call `final()` to get the hash of the concatenation.

The one-shot hash functions, `Digest`, `HmacSha256`, `Pbkdf2Sha256`, and
`RandBytes` are all partial. A call gives back a correct result or it raises —
it never gives back an incorrect one. The one-shot hash functions and `Digest`
raise when OpenSSL cannot perform the operation (e.g. FIPS mode disabling a
legacy algorithm). `HmacSha256`, `Pbkdf2Sha256`, and `RandBytes` also raise
when a length exceeds what OpenSSL's C `int` can hold.

`ConstantTimeCompare` compares two byte sequences in time independent of their
contents, for use when one side is a secret.
"""
