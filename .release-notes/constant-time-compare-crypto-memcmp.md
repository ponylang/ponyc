## ConstantTimeCompare now uses OpenSSL's CRYPTO_memcmp

LLVM may replace the pure-Pony XOR-accumulate loop in `ConstantTimeCompare` with an early exit once the accumulator is non-zero. That transformation preserves the return value but destroys the constant-time property. Pony has no `volatile` or compiler barrier to prevent it.

`ConstantTimeCompare` now calls OpenSSL's `CRYPTO_memcmp`, which is guaranteed constant-time. The `crypto` package already links libcrypto, so no new dependency is needed.
