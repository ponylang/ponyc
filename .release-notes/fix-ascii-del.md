## Add missing DEL (0x7F) to PonyCheck ASCII range primitives

The `ASCIINonPrintable` primitive in the `pony_test` package did not include DEL (0x7F). Because `ASCIIAll` and `ASCIIAllWithNUL` are built from it, they were also missing DEL — `ASCIIAll` had 126 bytes instead of 127, and `ASCIIAllWithNUL` had 127 instead of 128. `Generators.ascii()` uses `ASCIIAll` as its default range, so property tests using the default range never generated strings containing DEL.
