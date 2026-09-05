## Fix iftype capability narrowing inside generic return types

When a method on a generic class used `iftype` to narrow a type parameter's capability and returned a generic container parameterized by that type parameter, the compiler rejected the body with "function body isn't the result type."

For example, a method returning `MyBox[A]^` that produces `recover iso MyBox[A].create() end` inside an `iftype A <: Any val` branch was rejected.

This now compiles correctly. The fix applies to all capability constraints, including `#share`.
