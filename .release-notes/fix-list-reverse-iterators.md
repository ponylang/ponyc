## Fix List.rvalues() and List.rnodes() yielding only the first element

Both methods returned only the first element of the list instead of iterating in reverse; they now yield every element from last to first.

```pony
let l = List[String].from(["a"; "b"; "c"])
for v in l.rvalues() do
  env.out.print(v)
end
// was:  a
// now:  c, b, a
```
