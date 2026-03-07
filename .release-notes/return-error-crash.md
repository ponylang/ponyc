## Fix compiler crash on `return error`

Previously, writing `return error` in a function body would crash the compiler with an assertion failure instead of producing a diagnostic error. The compiler now correctly reports that a return value cannot be a control statement.
