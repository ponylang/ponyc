## Fix compiler crash on unterminated escape sequence at end of file

When a source file ended with a backslash inside a character or string literal — `let x = '\` or `let x = "\` at the last byte — `ponyc` aborted with an assertion failure instead of reporting a syntax error. The lexer now reports the incomplete escape and the unterminated literal as ordinary compile errors.
