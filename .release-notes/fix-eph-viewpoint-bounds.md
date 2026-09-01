## Fix overly conservative viewpoint adaptation bounds for ephemeral generic capabilities

When reading a field with a generic capability constraint (`#read`, `#alias`, or `#any`) through an ephemeral origin (`iso^` or `trn^`), the compiler produced overly conservative type bounds. This could reject valid programs or assign less capable types than the soundness criterion permits.

The compiler now returns the tightest bound the formal criterion validates. For example, reading a `#read` field through an `iso^` origin produces a `val` upper bound instead of `tag`.
