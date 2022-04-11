## Fix a runtime fault for Windows IOCP w/ memtrack messages.

When the runtime is compiled with `-DUSE_MEMTRACK_MESSAGES` on
Windows, the code path for asynchronous I/O events wasn't
initializing the pony context prior to its first use,
which would likely cause a runtime crash or other undesirable
behavior.

The `-DUSE_MEMTRACK_MESSAGES` feature is rarely used, and has
perhaps never been used on Windows, so that explains why
we haven't had a crash reported for this code path.

Now that path has been fixed by rearranging the order of the code.