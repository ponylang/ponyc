## Fix pony-doc displaying default parameter values

pony-doc displayed internal compiler token names like "call" and "reference" instead of the actual default parameter values written in source. A parameter declared as `fun apply(n: ISize = ISize.max_value())` appeared in the generated documentation with a default of "call" instead of `ISize.max_value()`. Similarly, `-1` appeared as "prefix".

Default values are now extracted from the original source text, so the documentation shows exactly what the user wrote.
