## Remove `json` package from the standard library

We accepted an [RFC](https://github.com/ponylang/rfcs/blob/main/text/0078-remove-json-package-from-stdlib.md) to remove the `json` package from the standard library.

Per the RFC, the motivation for removal was:

The json package is a regular source of difficult for Pony users. It is an ok API for some JSON handling needs and a poor API for other needs. The package
parses a String representation of some JSON into a mutable tree of objects.

This mutable tree is excellent for modifying the JSON, it is also an excellent representation for constructing a JSON document. However, it makes parsing a String into an shareable data structure that can be sent between actors very difficult.

The Pony ecosystem would be better served by not having an "official" JSON library that is part of the standard library. We would be better of by encouraging many different libraries that approach the problem differently. Sometimes, a purely functional approach like that taken by [jay](https://github.com/niclash/jay/) is called for. Other times, an approach that tightly controls memory usage and other performance oriented characteristics is better as seen in [pony-jason](https://github.com/jemc/pony-jason).

Having the current `json` package in the standard library given all its failings is a bad look. Better to have someone ask "what's the best way to handle JSON" then to blindly start using the existing package.

The existing package could be improved and it is best improved outside of the standard library and RFC process. The RFC process intentionally moves slowly. The `json` package is at best "beta" level software and would be greatly enhanced by a process that allows for more rapid change and experimentation. Further, it isn't essential to the community that a JSON library be part of the standard library. There's no cross library communication mechanism that depends on the existence of a standard JSON handling interface.

The library version of the `json` library is now available at [https://github.com/ponylang/json](https://github.com/ponylang/json).
