## Fix LSP hover showing on capability keywords

Hovering over a capability keyword (`iso`, `trn`, `ref`, `val`, `box`, `tag`) no longer shows hover information. Previously, hovering on the receiver cap in `fun ref foo()` or the type cap in `String val` would pop up method or type hover info, which was incorrect.
