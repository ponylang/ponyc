## Switch supported MacOS version to Ventura

We've switched our supported MacOS version from Monterey to Ventura.

"Supported" means that all ponyc changes are tested on Ventura rather than Monterey and our pre-built ponyc distribution is built on Ventura.

## Fixed a possible resource leak with `with` blocks

Previously, `with` blocks would allow the usage of `_` as a variable in the `with` parameters.

A `_` variable isn't usable within the block itself and `dispose` wasn't called on the variable as it isn't valid to call any methods on the special `_` variable.

The lack of `dispose` call on objects that might control resources could result in a resource leak.

We've addressed the issue by disallowing `_` as a variable name in `with` parameters.

This is a breaking change, albeit one we don't expect to have any "real-world" impact.

