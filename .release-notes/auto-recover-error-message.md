## Improve error message when a variable's capability changes inside a recover block

When a variable or parameter defined outside a recover block is used inside one, the compiler changes its capability to `tag`. If this causes an array element type mismatch, the error previously reported the changed capability without explaining where it came from.

The compiler now adds an informational line when this happens:

```
main.pony:5:12: f is Foo ref but is treated as Foo tag because it is used inside a recover block
```
