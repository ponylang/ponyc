## Add health check warnings to PonyCheck

PonyCheck now logs diagnostic warnings after a property run completes when it detects patterns that often signal a problem with the generator or property: a high filter rejection rate, an unusually large choice sequence, or a slow sample. The warnings never cause test failure — they appear in the test log when running with `--verbose`.

Three new fields on `PropertyParams` control the thresholds. All three default to values that avoid false positives on typical properties. Setting any threshold to 0 disables that check.

```pony
fun params(): PropertyParams =>
  PropertyParams(where
    max_filter_discard_ratio' = 5.0,
    max_choice_sequence_size' = 500,
    max_sample_nanos' = 500_000_000)
```
