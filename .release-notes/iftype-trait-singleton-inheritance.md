## Preserve inherited `iftype` specializations when composing traits

When one trait declared a method signature and another provided the same method with `iftype` specializations, listing the signature first could cause compilation to fail. The compiler now inherits the full method group in either order. A type can also override just the default or a matching specialization and inherit the other variants.
