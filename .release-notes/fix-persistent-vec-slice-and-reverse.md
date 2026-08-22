## Fix `Vec.slice` and `Vec.reverse` in `collections/persistent`

`Vec.slice` always returned an empty vector, whatever range it was given, and it ignored its `from` argument:

```pony
let v = Vec[USize].concat(Range(0, 10))
v.slice(2, 5).size() // was 0, is now 3
```

`Vec.reverse` never returned when called on an empty vector; it looped indefinitely instead.

Both have been fixed. `slice` now returns the requested range, saturating `to` at the size of the vector, and `reverse` on an empty vector returns an empty vector.
