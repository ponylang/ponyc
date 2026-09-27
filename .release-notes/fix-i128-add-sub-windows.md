## Fix compiler crash when using I128/U128 arithmetic on Windows

Compiling code that uses I128 or U128 addition, subtraction, or negation on Windows crashed during linking. The compiler now routes these operations through software implementations on Windows MSVC, matching what was already done for multiplication, division, and remainder.
