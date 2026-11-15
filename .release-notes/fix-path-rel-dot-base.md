## Fix `Path.rel` returning wrong results when the base is `"."` or `""`

`Path.rel(".", "a")` returned `"../a"` instead of `"a"`. Any base that cleaned to `"."` (including `""`) produced the same wrong result — the relative path gained a spurious `../` prefix. The documented contract `Path.join(to, Path.rel(to, target)) == target` failed for every target other than `"."` itself.
