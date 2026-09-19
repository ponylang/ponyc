## Add crypto package to the standard library

The `crypto` package provides cryptographic primitives backed by OpenSSL's libcrypto.

One-shot hash functions cover the common case where all the data is available at once:

```pony
use "crypto"

let hash = SHA256("Hello, World!")
env.out.print(ToHexString(hash))
```

Available one-shot functions: `MD4`, `MD5`, `RIPEMD160`, `SHA1`, `SHA224`, `SHA256`, `SHA384`, `SHA512`.

The streaming `Digest` class hashes data that arrives in pieces:

```pony
let d = Digest.sha256()?
d.append("Hello, ")?
d.append("World!")?
let hash = d.final()?
```

On OpenSSL 3.0.x and 4.0.x, `Digest.shake128` and `Digest.shake256` produce variable-length output.

The package also includes `HmacSha256` for message authentication, `Pbkdf2Sha256` for key derivation, `RandBytes` for cryptographically secure random bytes, and `ConstantTimeCompare` for timing-safe comparison.

If your code depended on `ponylang/ssl` for crypto, switch to `use "crypto"` with no code changes beyond the import path.
