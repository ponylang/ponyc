## Make HashFn.apply partial so callers can detect OpenSSL failure

The one-shot hash functions (`MD4`, `MD5`, `RIPEMD160`, `SHA1`, `SHA224`, `SHA256`, `SHA384`, `SHA512`) now raise an error when the underlying OpenSSL function fails instead of silently returning an all-zero array. The `HashFn` interface method is now partial to match.

Before:

```pony
let hash = SHA256("Hello World")
```

After:

```pony
let hash = SHA256("Hello World")?
```
