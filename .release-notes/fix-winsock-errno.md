## Fix wrong error codes for socket operations on Windows

On Windows, socket operations that failed could report stale or incorrect error codes. The failure was detected correctly, but the error code returned to the caller came from the wrong source. Error codes for socket operations on Windows are now correct.
