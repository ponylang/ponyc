## Fix segfault when using Generator.map with PonyCheck shrinking

Using `Generator.map` to transform values from one type to another would segfault during shrinking when a property test failed. For example, this program would crash:

```pony
let gen = recover val
  Generators.u32().map[String]({(n: U32): String^ => n.string()})
end
PonyCheck.for_all[String](gen, h)(
  {(sample: String, ph: PropertyHelper) =>
    ph.assert_true(sample.size() > 0)
  })?
```

The underlying compiler bug affected any code where a lambda appeared inside an object literal inside a generic method and was then passed to another generic method. The lambda's `apply` method was silently omitted from the vtable, causing a segfault when called at runtime.
