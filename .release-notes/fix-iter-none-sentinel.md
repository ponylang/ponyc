## Fix Iter.maybe, dedup, and take_while dropping None elements

`Iter.maybe`, `Iter.dedup`, and `Iter.take_while` silently dropped `None` elements when the element type was `None` or a union containing `None`, such as `(String | None)`.

All three methods now handle `None` elements correctly. One behavior change: `Iter[(T | None)].maybe(None)` now produces a one-element iterator containing `None` rather than an empty iterator.
