## Fix destructuring assignment for unions of same-arity tuples

Destructuring a union of tuples via assignment now works when every member of the union is a tuple with the same arity. Previously the compiler rejected it with "can't destructure a union using assignment, use pattern matching instead," even though the destructuring is type-safe. The most common trigger was iterating an array of lambda tuples without an explicit `as` type:

```pony
let handlers = [({(s: String): String => s + "!" },
                  {(s: String): String => s + "?" })
                ({(s: String): String => s + "." },
                  {(s: String): String => s + "," })]

for (exclaim, question) in handlers.values() do
  env.out.print(exclaim("hi") + question("hi"))
end
```

The workaround was to add an `as` clause to the array literal specifying the structural type. That is no longer necessary.
