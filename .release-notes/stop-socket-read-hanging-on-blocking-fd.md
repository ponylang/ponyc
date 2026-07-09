## Stop a socket read on a blocking file descriptor from hanging a program

A read on a socket could park a scheduler thread inside the read call, waiting for data that might never arrive. The runtime then never went idle and the program never exited.

The sockets `TCPConnection` and `UDPSocket` create are never in blocking mode, so this took a file descriptor from somewhere else: one opened over the C FFI, or one belonging to a library that closes and reuses descriptors itself, where a read for a closed connection can land on a descriptor number that a blocking socket has since taken over.

On Linux, macOS, and the BSDs, a socket read no longer waits for data to arrive, whatever mode the file descriptor is in. Windows offers no way to ask for that per read, so reads there still depend on the socket being non-blocking.
