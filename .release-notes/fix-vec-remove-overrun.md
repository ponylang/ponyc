## Fix data loss in `collections/persistent` `Vec.remove`

`Vec.remove(i, n)` destroyed elements it was not asked to remove whenever fewer than `n` elements followed index `i`.

```pony
use "collections/persistent"

actor Main
  new create(env: Env) =>
    try
      let v = Vec[USize].concat([as USize: 0; 1; 2; 3; 4].values())

      // asks to remove index 4 and two indices that do not exist
      let r = v.remove(4, 3)?

      // elements 2 and 3 were live, were not named, and are gone
      for x in r.values() do env.out.write(x.string() + " ") end
    end
```

```
0 1
```

The method removed `n` elements from the end of the vector before shifting the survivors down, so when the range ran past the end it took elements from before `i` and never put them back. No error was raised, and `size` was reduced to match the shortened vector, so nothing a caller could inspect disagreed.

This has been fixed. The count is now saturated: if fewer than `n` elements follow `i`, every element from `i` onward is removed and nothing before `i` is touched. The example above now prints `0 1 2 3`. This matches `Array.remove`, which `Vec.remove` mirrors, and `Vec.slice`, which already documented a saturated range. An index `i` that is out of bounds still raises an error.
