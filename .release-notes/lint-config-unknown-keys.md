## Report error when pony-lint encounters unknown rules in configuration

pony-lint now reports an error when a `.pony-lint.json` config file contains rule IDs or categories that don't match any known rule. Previously, a typo like `"stlye/line-length"` was silently ignored, leaving the rule in its default state with no indication that the config entry had no effect.
