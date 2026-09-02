## Fix compiler crash on array literals passed by name or to a call on a literal

The compiler crashed when an array literal appeared as an argument in certain positions: as an operand to a numeric literal (`1 + [as U8: 2]`), as a named argument to a callable object (`f(where x = [as U8: 1])`), or through update sugar when `update` is a field holding a callable object. These now compile or produce a normal error message.

Passing an array literal to an object with no `apply` method, or to a tuple, reported the error twice. Each now reports one error.
