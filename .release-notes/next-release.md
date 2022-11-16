## Don't include generated documentation "home file" in search index

We've updated the documentation generation to not include the "home file" that lists the packages in the search index. The "exclude from search engine" functionality is only available in the `mkdocs-material-insiders` theme.

The home file is extra noise in the index that provides no value. For anyone using the insiders theme, this will be an improvement in the search experience.

## Add support for erase left and erase line in term.ANSI

We've implemented [RFC #75](https://github.com/ponylang/rfcs/blob/main/text/0075-ansi-erase.md) that set out a means of updating `ANSI` in the `term` package to support not just ANSI erase right, but also erase left, and erase line.

Existing functionality continues to work as is. All existing erase right functionality will continue to work without breakage. The `erase` method has been augmented to take an option parameter for the direction to erase. `EraseRight` is the default, thus keeping existing behavior. `EraseLeft` and `EraseLine` options have been added as well.

To erase left, one would do:

```pony
ANSI.erase(EraseLeft)
```

To erase right, you could do either of the following:

```pony
ANSI.erase()
ANSI.erase(EraseRight)
```

And finally, to erase an entire line:

```pony
ANSI.erase(EraseLine)
```

