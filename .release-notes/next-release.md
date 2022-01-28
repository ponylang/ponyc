## Remove simplebuiltin compiler option

The ponyc option `simplebuiltin` was never useful to users but had been exposed for testing purposes for Pony developers. We've removed the need for the "simplebuiltin" package for testing and have remove both it and the compiler option.

Technically, this is a breaking change, but no end-users should be impacted.

## Fix return checking in behaviours and constructors

Our checks to make sure that the usage of return in behaviours and constructors was overly restrictive. It was checking for any usage of `return` that had a value including when the return wasn't from the method itself.

For example:

```pony
actor Main
  new create(env: Env) =>
    let f = {() => if true then return None end}
```

Would return an error despite the return being in a lambda and therefore not returning a value from the constructor.

