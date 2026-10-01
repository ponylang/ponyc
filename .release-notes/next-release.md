## Make HashFn.apply partial so callers can detect OpenSSL failure

The one-shot hash functions (`MD4`, `MD5`, `RIPEMD160`, `SHA1`, `SHA224`, `SHA256`, `SHA384`, `SHA512`) now raise an error when the underlying OpenSSL function fails instead of silently returning an all-zero array. The `HashFn` interface method is now partial to match.

Before:

```pony
let hash = SHA256("Hello World")
```

After:

```pony
let hash = SHA256("Hello World")?
```

## Add guarded devirtualization for multi-subtype interface dispatch

When a method is called through an interface or trait that has 2–4 concrete implementing types, the compiler now emits runtime type checks with direct calls instead of a vtable lookup, allowing method bodies to be inlined.

The optimization applies automatically wherever the compiler finds a small, closed set of concrete types behind an interface. The biggest impact is on hot loops that dispatch through iterators — `fold`, `map`, and other itertools combinators, where the `Iterator` interface typically has two concrete implementations (`Iter` and `Range`). On a pi-computation benchmark, the fold-vs-loop overhead dropped from 2.5x to parity.

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

