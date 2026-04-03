## Fix wrong code generation for `this->` in lambda type parameters

Using `this->` in a lambda type parameter (e.g., `{(this->A!): Bool}`) and then forwarding that lambda to another function produced wrong code at runtime. Depending on the program, you'd get incorrect results, wrong vtable dispatch, or a segfault. Calling the lambda directly in the same method worked fine — the bug only appeared when the lambda was passed along.

The root cause: lambda types are desugared into anonymous interfaces, and `this->` was copied verbatim into the interface. Once inside the interface, `this` referred to the interface's own receiver instead of the enclosing class's receiver, producing the wrong type for structural subtype checks.

`this->` is now resolved to the enclosing method's receiver capability during desugaring, so `{(this->A!)}` in a `box` method correctly becomes `{(box->A!)}`.
