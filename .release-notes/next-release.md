## Fix compiler crash with exhaustive match in generics

Previously, there was an interesting edge case in our handling of exhaustive match with generics that could result in a compiler crash. It's been fixed.

## Fixed parameter names not being checked

In 2018, when we [removed case functions](https://github.com/ponylang/ponyc/pull/2542) from Pony, we also removed the checking to make sure that parameters names were valid.

We've added checking of parameter names to make sure they match the naming rules for parameters in Pony.

