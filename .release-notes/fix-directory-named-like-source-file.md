## Fix crash when a directory is named like a Pony source file

Previously, if a directory inside a package was named like a Pony source file — that is, with a name ending in `.pony` — ponyc would abort with an out-of-memory error instead of compiling your program. Such directories are now ignored, like any other non-source entry in a package directory, and compilation proceeds normally.
