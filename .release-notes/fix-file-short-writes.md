## Fix File losing data on large writes

Writing a large amount of data to a `File` could silently lose the entire write and close the file. When the operating system wrote only part of the data in a single call (a short write), `File` raised an error, closed the file, and discarded all pending data, including the bytes already written.

Short writes are normal when the data exceeds the OS buffer size. `File` now retries after a short write, advancing past the bytes already written, until all data reaches the file or a real I/O error occurs.
