## Fix `Pbkdf2Sha256` raising on LibreSSL when the password is an empty `Array[U8]`

`Pbkdf2Sha256` raised an error on LibreSSL when the password was an empty `Array[U8]`. An empty password is valid in PBKDF2, and the same call with `""` as the password returned the correct key. On OpenSSL the empty `Array[U8]` already worked. The derivation now succeeds on both backends regardless of which empty type is used.
