## Fix `#share` capability constraint intersection

When intersecting a `#share` generic constraint with capabilities outside the `#share` set (like `ref` or `box`), the compiler incorrectly reported a non-empty intersection instead of recognizing that the capabilities are disjoint. This did not affect type safety — full subtyping checks always ran afterward — but the internal function returned incorrect intermediate results.
