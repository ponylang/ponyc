## Fixed an overflow in Time.nanos() values on Windows

Based on how were were calculating nanos based of the QueryPerformanceCounter and QueryPerformanceFrequency on Windows, it was quite likely that we would eventually overflow the U64 we used to represent nanos and "go backwards in time".

## Fix incorrect interaction between String/Array reserve and Pointer realloc

This was an interesting one. So, there's was an interesting interaction between String and Array's reserve and Pointer's realloc. Generally, it went like this:

`reserve` when it called `realloc` would give `realloc` a single value, the size of the newly reserved chunk of memory. This would result in `realloc` copying not just the in-use items from old memory to newly allocated memory, but in fact, the entire old memory chunk.

This could lead to very surprising results if `reserve` had been called by `undefined`. And in general, it could lead to suboptimal performance as we could end up copying more than was needed.

`realloc` has been updated to take to parameters, now, the amount of memory to alloc and the number of bytes to copy from old memory to new.

## Sort package types in documentation

In order to make documentation on a class, actor, primitive or struct easier to find, they are now sorted alphabetically inside their package.

## Adapt docgen to only depend the mkdocs material theme

We used to have our own mkdocs theme for ponylang documentation, it was based off of the mkdocs-material theme. This change puts the adaptationswe need into the mkdocs.yml file itself, so we don't depend on our own theme anymore, but only on the upstream mkdocs-material.

This is a **Breaking Change** in so far that users that want to generate documentation with ponyc now have to install the python package `mkdocs-material` instead of `mkdocs-ponylang` to generate their documentation in HTML form with mkdocs. The [library documentation github action](https://github.com/ponylang/library-documentation-action) will have the correct dependencies installed, so no changes should be needed when using it.

## Fix broken docgen on Windows

Generating documentation has been broken on windows: `ponyc` was not able to write the documentation files, due to path handling issues on windows. This is now fixed and docgen is working again on windows.
