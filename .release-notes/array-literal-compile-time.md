## Speed up type checking of large array literals

Type checking an array literal was quadratic in the number of elements, which made the type-check pass very slow for large literals -- a few thousand elements took many seconds. It is now linear in the number of elements: a 2000-element literal that spent about 93 seconds in the type checker now spends under half a second. Runtime behavior is unchanged.
