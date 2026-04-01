## Fix memory leak in Windows networking subsystem

Fixed a memory leak on Windows where an IOCP token's reference count was not decremented when a network send operation encountered backpressure. Over time, this could cause memory to grow unboundedly in programs with sustained network traffic.
