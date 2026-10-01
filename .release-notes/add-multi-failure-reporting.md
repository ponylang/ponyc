## Add multi-failure reporting to PonyCheck

PonyCheck can now collect multiple distinct failures in a single property test run. Set `max_distinct_failures` in `PropertyParams` to continue sampling after a failure is found and shrunk:

```pony
class iso MyProperty is Property[U8]
  fun name(): String => "my property"

  fun params(): PropertyParams =>
    PropertyParams(where max_distinct_failures' = 5)

  fun gen(): Generator[U8] => Generators.u8()

  fun ref property(sample: U8, h: PropertyHelper) ? =>
    // two independent bugs — both are reported in one run
    if (sample % 2) == 0 then error end
    if (sample % 7) == 0 then error end
```

Two failures are distinct when their shrunken choice sequences differ. When `max_distinct_failures` is 1 (the default), behavior is unchanged.

The `for_all` family of methods on `TestHelper` also accepts `PropertyParams`:

```pony
h.for_all[U8](
  recover val Generators.u8() end
  where params = PropertyParams(where max_distinct_failures' = 3))(
  {(sample, ph) ? =>
    if (sample % 2) == 0 then error end
  })?
```
