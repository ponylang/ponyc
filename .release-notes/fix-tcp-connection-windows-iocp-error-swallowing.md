## Fix Windows TCP connection silently leaking state on IOCP errors

When a Windows TCP connection's underlying socket failed during an IOCP write, or when the IOCP completion bookkeeping became inconsistent, the connection would silently leak its pending write state instead of closing. Subsequent writes would accumulate on a dead socket.

The connection now closes non-gracefully when these errors occur, matching the POSIX behavior.
