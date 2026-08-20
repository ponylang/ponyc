"""
# Ini package

The Ini package parses [INI file](https://en.wikipedia.org/wiki/INI_file)
formatted text. There is no single INI standard; implementations disagree on
comment characters, escaping, case sensitivity, and more. The dialect this
package parses is documented below.

There are two ways to use the package. `IniParse` is the common case: hand it
lines of text and get back a nested map of sections to key/value pairs. `Ini`
is the streaming parser that sits underneath `IniParse`; use it directly when
you want to handle entries as they are parsed, react to errors, or stop early.

# Format support

Sections are written `[name]` on a line of their own. Anything after the
closing `]` is discarded, so `[name] ; trailing` is the section `name`.
Whitespace around the section name is trimmed, so `[ name ]` is the section
`name`. `[]` and `[   ]` are both the empty-string section. Internal
whitespace is preserved: `[a b]` is the section `"a b"`. Section nesting is
not interpreted: `[a.b]` is a section literally named `a.b`. Keys that appear
before the first section header belong to the section named `""`.

Comments start with `;` or `#`. A line whose first non-whitespace character is
`;` or `#` is a comment and is skipped. A comment may also appear at the end
of a value, but only when the `;` or `#` is preceded by a space or tab in the
trimmed value: `key = value ; comment` has the value `value`, while
`key = value;comment` has the value `value;comment`. The check happens after
the value is trimmed, so a `;` or `#` that is the first non-whitespace
character of the value is treated as a literal value character — `key = ;c`
has the value `;c`, and the same is true of `key =    ;c`. The parser looks
for the first `;` in the value and, only if there is no `;` at all, the
first `#`; it then checks that single marker for a preceding space or tab.
A `;` that is not a comment therefore shadows any later comment, so
`key = a;b ; c` has the value `a;b ; c` and `key = a;b # c` has the value
`a;b # c`. When the comment is removed, any whitespace between the value and
the marker is removed too, so `key = value   ;comment` has the value `value`.
A `;` or `#` to the left of the key/value delimiter is an ordinary character;
comments are not recognised inside keys.

Keys and values are separated by `=`, or by `:` when the line contains no
`=`. The `=` wins even when a `:` appears earlier on the line, so `a:b=c` is
the key `a:b` with the value `c`. Whitespace around the key and around the
value is trimmed. A line with neither `=` nor `:` is an error. The key may be
empty: `=value` is the empty key with the value `value`. Values are otherwise
taken literally. There is no quoting and there are no escape sequences; quote
and backslash characters are part of the value, and the inline comment rules
above apply inside what looks like a quoted string: `key = "a ; b"` has the
value `"a`, because the ` ;` is treated as the start of a comment.

Blank lines are ignored. Each line is trimmed before anything else happens, so
leading and trailing whitespace at the line level never matters.

`IniParse` stores sections and keys in a case-sensitive map: `[Section]` and
`[section]` are different sections. `Ini` does no matching of its own; it
hands every section, key, and value to your `IniNotify` exactly as parsed.

Malformed input produces an `IniError`. How parsing responds to one depends
on which entry point you use; see `Ini` and `IniParse` for the details.

# Not supported

* Multi-line entries.
* Quoted values.
* Escape sequences.
* Nested sections.

# Example code
```pony
// Parses the file 'example.ini' in the current working directory
// Output all the content
use "ini"
use "files"

actor Main
  new create(env: Env) =>
    try
      let ini_file = File(FilePath(FileAuth(env.root), "example.ini"))
      let sections = IniParse(ini_file.lines())?
      for section in sections.keys() do
        env.out.print("Section name is: " + section)
        for key in sections(section)?.keys() do
          env.out.print(key + " = " + sections(section)?(key)?)
        end
      end
    end
```
"""
