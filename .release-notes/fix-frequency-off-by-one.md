## Fix `Generators.frequency` over-weighting the first entry

`Generators.frequency` in `pony_test` picks a value from a weighted set of generators. It over-weighted its first entry — a first entry with weight 0 was chosen when it should never have been, and all other entries were skewed toward the first entry by one extra share of the total weight.

Each entry is now chosen with probability `weight / weight_sum`. Entries with weight zero are never chosen. When all weights are zero, `generate` errors.

Saved regression inputs that used `frequency` may replay differently. Delete `.ponytest/` and let regressions be rediscovered on the next failing run.
