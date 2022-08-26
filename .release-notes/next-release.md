## Fix bug in `StdStream.print`

When printing via `StdStream.print` strings containing null bytes, the standard library was printing the string until the first null byte and then padding the printed string with space characters until the string size was reached.

This bug was introduced in Pony 0.12.0 while fixing a different printing bug.

We've fixed the bug so that printing strings with null bytes works as expected.

