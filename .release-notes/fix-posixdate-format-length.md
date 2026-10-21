## Fix PosixDate.format raising on valid formats whose output is 8192 bytes or longer

`PosixDate.format` raised an error on any valid format string that produced 8192 bytes or more of output. Formats producing shorter output were unaffected.

```pony
PosixDate(0).format("a" * 8192)?  // was: error raised
                                    // now: 8192-byte string of "a"s
PosixDate(0).format("%Y" * 2048)? // was: error raised
                                    // now: "1970" repeated 2048 times
```
