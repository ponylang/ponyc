## Fix soundness hole in match capture bindings

Match `let` bindings with viewpoint-adapted or generic types could bypass the compiler's capability checks, allowing creation of multiple `iso` references to the same object. A direct `let x: Foo iso` capture was correctly rejected, but `let x: this->B iso` and `let x: this->T` (where T could be iso) slipped through because viewpoint adaptation through `box` erases the ephemeral marker that the existing check relies on to detect unsoundness.

The compiler now checks whether a capture type has a capability that would change under aliasing (iso, trn, or a generic cap that includes them) and rejects the capture when the match expression isn't ephemeral. Previously-accepted code that hits this check was unsound and could segfault at runtime.

### How to fix code broken by this change

Consume the match expression so the discriminee is ephemeral:

Before (unsound, now rejected):

```pony
match u
| let ut: T =>
  do_something(consume ut)
else
  (consume u, default())
end
```

After:

```pony
match consume u
| let ut: T =>
  do_something(consume ut)
| let uu: U =>
  (consume uu, default())
end
```

The `else` branch becomes `| let uu: U =>` because `u` is consumed and no longer available. For field access, use a destructive read (`match field_name = sentinel_value`) to get an ephemeral result.
