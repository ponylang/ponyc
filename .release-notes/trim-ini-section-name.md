## Trim whitespace from INI section names

Previously, the INI parser left whitespace inside `[ name ]` as part of the section name, so an input like:

```ini
[ section ]
key = value
```

produced a section named `" section "`. Looking it up as `"section"` missed.

The parser already trims whitespace from lines, keys, and values. Not trimming section names was inconsistent rather than a deliberate dialect choice.

Section names are now trimmed of leading and trailing whitespace. `[ name ]` parses as `name`. Internal whitespace is preserved — `[a b]` is still `"a b"`. `[]` and `[   ]` are both the empty-string section.

This is a behavior change: any existing INI input that relied on the old quirk to distinguish sections by surrounding whitespace (e.g., treating `[section]` and `[ section ]` as different sections) will now see those sections collapse into one. Because `IniParse` overwrites duplicate keys with the last value seen, keys from the earlier section can be silently overwritten.
