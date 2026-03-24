## Fix code generation failure for iftype with union return type

Previously, using `iftype` in a function that returns a union type caused an internal compiler error during code generation:

```pony
actor Main
  new create(env: Env) =>
    test[U8]()

  fun test[T: U8](): (U8 | None) =>
    iftype T <: U8 then
      0
    end
```

```
Error:
main.pony:5:3: internal failure: code generation failed for method Main_ref_test_U8_val_o
```

This has been fixed. Functions using `iftype` with union return types now compile and run correctly.
