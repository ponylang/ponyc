## Fix File/CreateFile callers leaving stale tails when writing shorter content

Several callers opened existing files with `File(path)` or `CreateFile(path)` and wrote from position 0 without truncating first. When new content was shorter than old content, the tail of the old data remained in the file. Affected callers: readline history saving, the pony_test regression database, pony-doc output files, and the mandelbrot benchmark.

Each caller now truncates the file with `set_length(0)` before writing.
