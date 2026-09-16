## Add classification API to PonyCheck

Property-based tests can now report how their generated inputs distribute across categories. We added four methods to `PropertyHelper` following the classify/tabulate/cover pattern from QuickCheck and Hypothesis:

```pony
use "pony_check"

class iso MyProperty is Property1[U8]
  fun name(): String => "my property"

  fun gen(): Generator[U8] => Generators.u8(0, 100)

  fun ref property(sample: U8, h: PropertyHelper) =>
    // flat label — reported as a percentage of all samples
    h.classify(if sample < 10 then "small" else "large" end)

    // grouped label — independent counter under a named heading
    h.tabulate("parity",
      if (sample %% 2) == 0 then "even" else "odd" end)

    // coverage requirement — fails the property without shrinking
    // when fewer than 5% of samples carry the label
    h.cover(sample < 10, "small", 5.0)

    // convenience — classifies using value.string()
    h.collect(sample)
```

After all samples run, the runner prints a distribution table. A `ClassificationNotify` callback gives programmatic access to the flat and tabulated counts.
