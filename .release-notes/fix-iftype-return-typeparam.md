## Fix iftype branches not satisfying type parameter return types

Concrete return values inside iftype branches were rejected with "no lower bounds" when the function's return type was a type parameter whose constraint is a union type, even when each branch returned the correct type for its narrowed constraint.

```pony
type FooBar is (Foo | Bar)

primitive Helper
  fun test[J: FooBar val](): J ? =>
    iftype J <: Foo val then recover Foo end  // was rejected
    elseif J <: Bar val then recover Bar end
    else error
    end
```

This code now compiles and runs correctly.
