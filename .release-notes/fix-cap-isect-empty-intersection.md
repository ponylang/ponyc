## Fix cap_isect_constraint returning incorrect capability for empty intersections

When a type parameter was constrained by an intersection of types with incompatible capabilities (e.g., `ref` and `val`), the compiler incorrectly computed the effective capability as `#any` (the universal set) instead of recognizing that no capability satisfies both constraints. This could cause the compiler to silently accept type parameter constraints that have no valid capability, rather than reporting an error.

The compiler now correctly detects empty capability intersections and reports "type parameter constraint has no valid capability" when the intersection of capabilities in a type parameter's constraint is empty. This also fixes incorrect results for `iso` intersected with `#share` and `#share` intersected with concrete capabilities outside its set, which were caused by missing `break` statements and an incorrect case in the capability intersection logic.
