## Reverts Ansi.erase to erase to the right, not the left

With the release of 0.49.0, we changed `Ansi.erase` to be erasing left rather than right. This made sense based on the comment on the `Ansi.erase` method. However, the actual usage in the standard library of `Ansi.erase` was expecting it to erase to the right. Because of this change, since 0.49.0, the readline support in the `term` package has been broken.

We've reverted the changes from the [original PR](https://github.com/ponylang/ponyc/pull/4022) and updated the comment on `Ansi.erase` to note that it erases to right, not the left.

We would welcome an RFC to discuss supporting both left and right erasure as part of the `Ansi` primitive.
