## Truncate oversized UDP datagrams on Windows instead of dropping them

On Windows, a UDP datagram larger than the receiving socket's read buffer was delivered to `UDPSocket`'s notifier as an empty array, silently dropping the entire datagram. On Linux, macOS, and the BSDs the same datagram is delivered truncated to the buffer's first bytes, with only the excess discarded.

Windows now matches that behavior: an oversized datagram is delivered truncated to the buffer's first bytes instead of being dropped entirely. This does not prevent data loss -- the bytes beyond the buffer are still discarded, on every platform. To receive a datagram whole, size the `UDPSocket` read buffer (the `size` argument to the listen constructors) to your protocol's largest expected datagram.
