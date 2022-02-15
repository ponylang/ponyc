## Fix soundness bug introduced

We previously switched our underlying type system model. In the process, a
soundness hole was introduced where users could assign values to variables with
an ephemeral capability. This could lead to "very bad things".

The hole was introduced in May of 2021 by [PR #3643](https://github.com/ponylang/ponyc/pull/3643). We've closed the hole so that code such as the following will again be rejected by the compiler.

```pony
let map: Map[String, Array[String]] trn^ = recover trn Map[String, Array[String]] end
```

## Add workaround for compiler assertion failure in Promise.flatten_next

Under certain conditions a specific usage of `Promise.flatten_next` and `Promise.next` could trigger a compiler assertion, whose root-cause is still under investigation. 

The workaround avoids the code-path that triggers the assertion. We still need a full solution.

