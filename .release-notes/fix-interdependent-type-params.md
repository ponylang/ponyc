## Fix type checking failure for interfaces with interdependent type parameters

Previously, interfaces with multiple type parameters where one parameter appeared as a type argument to the same interface would fail to type check:

```pony
interface State[S, I, O]
  fun val apply(state: S, input: I): (S, O)
  fun val bind[O2](next: State[S, O, O2]): State[S, I, O2]
```

```
Error:
type argument is outside its constraint
  argument: O #any
  constraint: O2 #any
```

The compiler replaced type variables one at a time during reification, so replacing `S` with its value could inadvertently transform a different parameter's constraint before that parameter was processed. This has been fixed by replacing all type variables in a single pass.
