# The `_p` partiality marker's collision-freedom depends on no type mangle ending in `'p'`

`make_mangled_name` (`src/libponyc/reach/reach.c`) appends a trailing `_p` to a
partial method's mangled name, so the painter assigns it a vtable slot separate
from the matching non-partial method. This matters because partiality is part of
a method's calling convention — a partial method's slot returns `{T, i1}` while a
non-partial one returns `T` — so the two must never share a slot, or a method
lands in a slot whose ABI doesn't match its body, a silent calling-convention bug
in generated code.

That segregation is collision-free only because no type mangle (`t->mangle`,
assigned later in `reach.c`) is, or ends in, `'p'`: a non-internal method's base
mangle ends in its result type's mangle, and `'p'` is not in the type-mangle
character set, so a non-partial method's mangle never ends in `'p'` while a
partial one always ends in the `_p` marker. Add a `t->mangle` value ending in
`'p'`, or change the marker to a character that IS a type mangle, and a
non-partial method can collide with a partial one in the same slot, reviving the
ABI crash the segregation prevents. Both ends carry comments pointing at each
other (the collision-free note in `make_mangled_name` and the reciprocal note at
the `t->mangle` assignments).

Run: `ctest --preset debug -R 'libponyc\.tests|libponyrt\.tests|full-programs'`.
