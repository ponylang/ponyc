## Add PonyCheck to the standard library

PonyCheck, a property based testing library, has been added to the standard library. PonyCheck was previously its own project but has been merged into the standard library.

For the most part existing PonyCheck tests will continue to work once you make a few minor changes.

### Remove PonyCheck from your corral.json

As PonyCheck is now part of the standard library, you don't need to use `corral` to fetch it.

### Change the package name

Previously, the PonyCheck package was called `ponycheck`. To conform with standard library naming conventions, the package has been renamed to `pony_check`. So anywhere you previously had:

```pony
use "ponycheck"
```

You'll need to update to:

```pony
use "pony_check"
```

### Update the PonyCheck primitive name

If you were using the `Ponycheck` primitive, you'll need to change it to it's new name `PonyCheck`.
