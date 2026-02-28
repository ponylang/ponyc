## Add control structure alignment lint rule

pony-lint now checks that control structure keywords are vertically aligned. The `style/control-structure-alignment` rule verifies that `if`/`elseif`/`else`/`end`, `for`/`else`/`end`, `while`/`else`/`end`, `try`/`else`/`then`/`end`, `repeat`/`until`/`else`/`end`, `with`/`end`, and `ifdef`/`elseif`/`else`/`end` all start at the same column. Single-line structures are exempt.

```pony
// Flagged: else and end not aligned with if
if condition then
  do_something()
    else
  do_other()
      end

// Clean: all keywords at the same column
if condition then
  do_something()
else
  do_other()
end
```
