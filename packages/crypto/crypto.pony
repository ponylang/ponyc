"""
Cryptographic primitives built on OpenSSL's libcrypto.

For a single input, use the one-shot hash functions (`MD5`, `SHA256`, etc.) —
they take a `ByteSeq` and return `Array[U8] val` in one call, and they cannot
fail. For input that arrives in pieces, use `Digest`: construct one for the
algorithm you want, feed it with `append()`, and call `final()` to get the
hash of the concatenation.

`HmacSha256`, `Pbkdf2Sha256`, `RandBytes`, and the `Digest` constructors,
`append`, and `final` are partial. They raise when OpenSSL cannot do what was
asked, or when a length exceeds what OpenSSL's C `int` can hold. A call here
gives back a correct result or it raises — it never gives back an incorrect
one.

`ConstantTimeCompare` compares two byte sequences in time independent of their
contents, for use when one side is a secret.
"""
