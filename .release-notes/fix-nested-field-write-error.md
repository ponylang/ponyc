## Fix error message for nested field writes in immutable methods

Writing to a field through another field — `my_object.x = 1` where `my_object` is itself a field on `this` — inside a `fun box`, `fun val`, or `fun tag` method produced the generic "left side is immutable" error. The compiler now reports "cannot write to a field in a box function. If you are trying to change state in a function use fun ref," matching the message already given for direct field writes like `x = 42`.
