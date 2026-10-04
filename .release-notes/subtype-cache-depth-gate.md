## Fix subtype cache doing unnecessary work on every subtype check

The compiler's subtype cache bounds the cost of deeply recursive type alias networks. It ran its most expensive step on every subtype check, even though normal checks are shallow and never benefit from the cache. The cache now activates only at the recursion depths where it helps, eliminating the overhead for the common case. Measured improvement: ~3% overall compilation time on stdlib.
