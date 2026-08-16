## Add the style/testlist-nodoc pony-lint rule

`style/testlist-nodoc` flags types that provide `TestList` without a `\nodoc\` annotation. `TestList` implementations exist only to register tests with PonyTest and don't belong in generated documentation.

```pony
// Flagged — missing \nodoc\
primitive _MyTests is TestList
  new make() => None
  fun tag tests(test: PonyTest) => None

// Clean
primitive \nodoc\ _MyTests is TestList
  new make() => None
  fun tag tests(test: PonyTest) => None
```

The rule is on by default. Disable it with `--disable style/testlist-nodoc` or in `.pony-lint.json`.
