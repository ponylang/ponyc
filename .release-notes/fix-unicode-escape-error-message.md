## Fix error message for out-of-range unicode escape printing rest of source file

The error message for a `\U` escape above 0x10FFFF included everything from the escape to the end of the source file instead of just the escape sequence. A file containing `"\U110000"` followed by more code produced an error quoting all of that trailing code inside the message. The message now quotes only the escape.
