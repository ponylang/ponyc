## Improve error message when writing to a field in an immutable method

Writing to a field inside a `fun` (which defaults to `box` receiver capability) used to produce the generic error "left side is immutable." The compiler now reports "cannot write to a field in a box function. If you are trying to change state in a function use fun ref."
