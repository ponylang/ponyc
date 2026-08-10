## Fix slow stdin reads from redirected files on Windows

On Windows, reading a file redirected into stdin (`program < file.txt`) was about nine times slower than necessary. The runtime now uses the same approach Linux and macOS use for redirected files, eliminating the overhead.
