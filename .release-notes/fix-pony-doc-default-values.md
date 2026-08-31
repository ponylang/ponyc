## Fix pony-doc default parameter value display

Generated documentation displayed token keywords instead of actual default parameter values. A default of `ISize.max_value()` appeared as "call" and `-1` appeared as "reference" in the docs:

```pony
fun apply(to: ISize = ISize.max_value()): None
```

```
Parameters:
  to: ISize val = call
```

Default parameter values now display correctly in generated documentation.
