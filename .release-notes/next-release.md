## Fix bug in `StdStream.print`

When printing via `StdStream.print` strings containing null bytes, the standard library was printing the string until the first null byte and then padding the printed string with space characters until the string size was reached.

This bug was introduced in Pony 0.12.0 while fixing a different printing bug.

We've fixed the bug so that printing strings with null bytes works as expected.

## Enhance checking for identity comparison with new objects

Consider the following program:

```pony
class C

actor Main
  new create(env: Env) =>
    env.out.print(if C is C then "C is C" else "C is NOT C" end)
```

This will fail to compile with the message `identity comparison with a new object will always be false`.

Nevertheless, the checking wasn't exhaustive and some cases weren't covered, like the one in the following example:

```pony
class C

actor Main
  new create(env: Env) =>
    env.out.print(if C.create() is C.create() then "C is C" else "C is NOT C" end)
```

We've made the check exhaustive and we believe it now covers all possible cases.

