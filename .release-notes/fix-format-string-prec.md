## Fix Format.apply not applying prec for strings

`Format.apply` did not apply the `prec` parameter. `Format("hello" where prec = 2)` returned `"hello"` instead of `"he"`. When `width` was also set, output was wider than `width` — `Format("hello" where prec = 2, width = 4, align = AlignRight)` returned `"  hello"` (7 characters) instead of `"  he"` (4 characters).
