## Add Pony Language Server to the Ponyc distribution

We've moved the Pony Language Server that previously was distributed as it's own project to the standard ponyc distribution. This means that the language server will track changes in ponyc. Going forward, the language server that ships with a given ponyc will be fully up-to-date with the compiler and most importantly, it's version of the standard library.

## Handle Bool with exhaustive match

Previously, this function would fail to compile, as true and false were not seen as exhaustive, resulting in the match block exiting and the function attempting to return None.

```pony
  fun fourty_two(err: Bool): USize =>
    match err
    | true => return 50
    | false => return 42
    end
```

We have modified the compiler to recognize that if we have clauses for both true and false, that the match is exhaustive.

Consequently, you can expect the following to now fail to compile with unreachable code:

```pony
  fun forty_three(err: Bool): USize =>
    match err
    | true => return 50
    | false => return 43
    else
      return 44 // Unreachable
    end
```

