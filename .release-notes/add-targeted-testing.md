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
