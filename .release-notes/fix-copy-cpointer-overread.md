## Fix String.copy_cpointer reading one byte past the source buffer

`String.copy_cpointer` copied `len + 1` bytes from the source pointer, assuming a null terminator existed at position `len`. The method's contract is to copy a fixed number of bytes — not a C string — so any source buffer without a trailing null was overread by one byte. This affected FFI callbacks that receive length-delimited buffers, such as the OpenSSL ALPN select callback.
