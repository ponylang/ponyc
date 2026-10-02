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

## Add multicast convenience methods to UDPSocket

`UDPSocket` now has methods for common multicast operations: joining and leaving groups, setting TTL and hop limits, enabling loopback, and selecting the outgoing interface. Each method takes plain types (a string address, an integer) and returns 0 on success or a non-zero errno.

```pony
udp.join_multicast_group_v4("239.1.2.3", "127.0.0.1")
udp.set_multicast_loopback_v4(true)
udp.set_multicast_ttl(4)
```

Previously, multicast setup required packing C structs in network byte order and selecting the correct protocol-level constants by hand through `setsockopt`. The convenience methods handle struct layout and platform differences internally. IPv4 and IPv6 have separate methods because the underlying socket options take different parameter types.

## Add \inline\, \inline(N)\, and \noinline\ annotations

Three new annotations give control over LLVM's inlining decisions on `fun` methods.

`\inline\` forces the function to be inlined at every call site:

```pony
primitive Foo
  fun \inline\ hot_path(): U64 => 42
```

`\inline(N)\` raises LLVM's inline cost threshold to `N` for the function, making it more likely to be inlined without forcing it:

```pony
primitive Foo
  fun \inline(500)\ fairly_large(): U64 => 42
```

`\noinline\` prevents the function from being inlined:

```pony
primitive Foo
  fun \noinline\ cold_path(): U64 => 42
```

These annotations apply only to `fun` declarations — not to behaviors or constructors. The compiler raises the inline threshold automatically on functions that use direct-call dispatch guards for interfaces; an explicit annotation on such a function overrides that automatic threshold.

## Add targeted testing to PonyCheck

Property tests can now steer generation toward inputs that maximize a score. Call `h.target(score)` inside a property body, and PonyCheck biases future samples toward higher-scoring regions of the input space:

```pony
class iso FindLargestGap is Property[Array[U8] val]
  fun name(): String => "find largest gap"

  fun gen(): Generator[Array[U8] val] =>
    Generators.array_of[U8](Generators.u8() where min = 2, max = 20)

  fun ref property(sample: Array[U8] val, h: PropertyHelper) =>
    var max_gap: U8 = 0
    try
      var i: USize = 1
      while i < sample.size() do
        let gap =
          if sample(i)? > sample(i - 1)? then
            sample(i)? - sample(i - 1)?
          else
            sample(i - 1)? - sample(i)?
          end
        if gap > max_gap then max_gap = gap end
        i = i + 1
      end
    end
    h.target(max_gap.f64())
    h.assert_true(max_gap < 200)
```

Use separate labels to track multiple independent objectives. To minimize a score, negate it.

