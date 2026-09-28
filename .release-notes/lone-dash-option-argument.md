## Accept a lone dash as a long option argument

Compiler and runtime long options that take arguments now accept a separate `-` as their value. For example, `--ponytracingoutput -` now sends tracing output to stdout; previously, it reported a missing argument and required `--ponytracingoutput=-`. The `=-` form still works. Other values beginning with `-`, such as `-x` or `--flag`, still require the `=` form.
