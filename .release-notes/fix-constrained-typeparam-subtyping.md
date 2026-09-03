## Fix type parameters constrained by other type parameters

When one type parameter was constrained by another, the compiler rejected code that treated the constrained parameter as a subtype of its constraint:

```pony
class A[X, Y: X]
  fun foo(y: Y) =>
    let x: X = consume y
```

The constraint `Y: X` declares that `Y` is a subtype of `X`, and the compiler now accepts this. Transitive chains also work: `Z: Y` and `Y: X` together imply `Z` is a subtype of `X`.
