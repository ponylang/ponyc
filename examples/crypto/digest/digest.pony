"""
Hashes data in chunks using the streaming `Digest` API from the `crypto`
package.

Creates a SHA256 digest, appends data in two pieces, and finalizes to
produce the hash. On OpenSSL 3.0.x and 4.0.x, also demonstrates
`Digest.shake256` for variable-length output.
"""
