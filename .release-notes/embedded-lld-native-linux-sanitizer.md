## Use embedded LLD for native Linux sanitizer builds

When ponyc is built with sanitizers (such as `address_sanitizer` or `undefined_behavior_sanitizer`) on Linux, it now links the programs it compiles with its built-in LLD linker, the same as every other build, instead of falling back to your system C compiler to perform the link. Sanitizer-enabled native Linux compilation no longer depends on having an external compiler driver present and usable as a linker.
