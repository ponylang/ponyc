## Change stack_depth_t to size_t on OpenBSD

The definition of stack_depth_t was changed from int to size_t to support compiling on OpenBSD 7.6.

ponyc may not be compilable on earlier versions of OpenBSD.

## Make sure scheduler threads don't ACK the quiescence protocol CNF messages if they have an actor waiting to be run

Prior to this, the pinned actor thread could cause early termination/quiescence of a pony program if there were only pinned actors active. This change fixes the issue to ensure that pony programs with only pinned actors active will no longer terminate too early.

## Apply default options for a CLI parent command when a sub command is parsed

In the CLI package's parser, a default option for a parent command was ignored when a subcommand was present. This fix makes sure that parents' defaults are applied before handling the sub command.

## Fix compiler crash from `match` with extra parens around `let` in tuple

When matching on a tuple element within a union type, the compiler would crash when extra parentheses were present.

The following code fails to compile because of `(123, (let x: I32))` in the `match`. The extra parentheses should be ignored and treated like `(123, let x: I32)`.

```pony
let value: (I32 | (I32, I32)) = (123, 42)

match value
| (123, (let x: I32)) => x
end
```

## Fix soundness problem when matching `iso` variables

We previously switched our underlying type system model. In the process, a
soundness hole was introduced. The following code that should not compile was accepted by the compiler:

```pony
class Bad
  let _s: String iso

  new iso create(s: String iso) =>
    _s = consume s

  fun ref take(): String iso^ =>
    match _s
    | let s': String iso => consume s'
    end
```

The code should not compile because `let s': String iso` is aliasing `_s` an iso field. Consuming an aliases iso shouldn't return an iso^.

The take-away is that "very bad things could happen" and the data race freedom guarantees of the Pony compiler were being violated.

We've closed the soundness hole. We recommend all Pony users update to the this release as soon as possible.

