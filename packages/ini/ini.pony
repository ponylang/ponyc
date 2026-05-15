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
closing `]` is discarded, so `[name] ; trailing` is the section `name`. The
text between the brackets is taken as-is and is not trimmed, so `[ name ]` is
the section `" name "`. `[]` is a valid section whose name is the empty
string. Section nesting is not interpreted: `[a.b]` is a section literally
named `a.b`. Keys that appear before the first section header belong to the
section named `""`.

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
primitive IniIncompleteSection
  """
  An `IniError` reported when a line that begins with `[` has no closing `]`.
  """

primitive IniNoDelimiter
  """
  An `IniError` reported when a line that is not a comment or section header
  contains neither `=` nor `:`.
  """

type IniError is
  ( IniIncompleteSection
  | IniNoDelimiter
  )
  """
  The set of errors `Ini` can report while parsing.
  """

interface IniNotify
  """
  Handler interface for the streaming parser `Ini`. The parser calls these
  methods as it works through the input. Only `apply` must be implemented;
  `add_section` and `errors` ship default implementations that accept all
  sections and continue past errors.
  """
  fun ref apply(section: String, key: String, value: String): Bool
    """
    Called for every key/value pair. Keys that appear before any section
    header are reported with `section` set to the empty string. Return
    `false` to halt processing.
    """

  fun ref add_section(section: String): Bool =>
    """
    Called for every `[section]` header. The implicit empty-string section
    that holds keys appearing before any header does not trigger this
    callback; only a literal header line (including `[]`) does. Return
    `false` to halt processing.
    """
    true

  fun ref errors(line: USize, err: IniError): Bool =>
    """
    Called for each malformed line. `line` is the 1-based line number where
    the error was detected. Return `false` to halt parsing immediately;
    `true` to keep going. Even when this returns `true` and parsing
    continues, `Ini.apply` will return `false` at the end.
    """
    true

primitive Ini
  """
  A streaming parser for INI formatted lines of text.

  Lines are pulled from the iterator one at a time and dispatched to an
  `IniNotify` handler as section headers, key/value pairs, or errors. The
  current section name is the only piece of parsed input that persists
  across handler calls; everything else (the line number reported with
  errors, the running success/failure status returned at the end) is
  internal bookkeeping the handler does not see.
  """
  fun apply(lines: Iterator[String box], f: IniNotify): Bool =>
    """
    Parse `lines` and call `f` for each section header and key/value pair.
    Returns `true` when parsing finishes with no errors, `false` otherwise.

    When the parser hits a malformed line it calls `f.errors`. If `errors`
    returns `true` parsing continues, but the final return value is still
    `false`. If `errors` returns `false` parsing stops immediately and
    `apply` returns `false`.

    `f.apply` and `f.add_section` can also stop parsing early by returning
    `false`. In that case `apply` returns whatever the parse status was at
    the point of the stop: `true` if no errors had been seen, `false` if any
    had.
    """
    var section = ""
    var lineno = USize(0)
    var ok = true

    for line in lines do
      lineno = lineno + 1
      var current = line.clone()
      current.strip()

      if current.size() == 0 then
        continue
      end

      try
        match current(0)?
        | ';' | '#' =>
          // Skip comments.
          continue
        | '[' =>
          try
            current.delete(current.find("]", 1)?, -1)
            current.delete(0)
            section = consume current
            if not f.add_section(section) then
              return ok
            end
          else
            ok = false

            if not f.errors(lineno, IniIncompleteSection) then
              return false
            end
          end
        else
          try
            let delim = try
              current.find("=")?
            else
              current.find(":")?
            end

            let value = current.substring(delim + 1)
            value.strip()

            current.delete(delim, -1)
            current.strip()

            try
              let comment = try
                value.find(";")?
              else
                value.find("#")?
              end

              match value(comment.usize() - 1)?
              | ' ' | '\t' =>
                value.delete(comment, -1)
                value.rstrip()
              end
            end

            if not f(section, consume current, consume value) then
              return ok
            end
          else
            ok = false

            if not f.errors(lineno, IniNoDelimiter) then
              return false
            end
          end
        end
      end
    end
    ok
