## Fix TCP connections hanging on Windows

On Windows, a busy TCP connection could hang: one that had several writes queued up would, after sending its data, stop receiving and never finish -- the connection stayed stuck and the program sat idle. This was most likely on connections moving a lot of data at once. Affected connections now continue and complete normally.
