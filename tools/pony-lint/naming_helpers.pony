primitive NamingHelpers
  """
  Validation and transformation helpers for Pony naming conventions.

  CamelCase is used for type names: uppercase start (after optional leading
  `_`), no underscores, alphanumeric characters only.

  snake_case is used for member names: lowercase start (after optional leading
  `_`), lowercase letters, digits, and underscores only.
  """

  fun is_camel_case(name: String val): Bool =>
    """
    Returns true if `name` follows CamelCase conventions: starts with an
    uppercase letter (after optional leading `_`), contains only alphanumeric
    characters, and has no underscores after the optional prefix. A trailing
    prime (`'`) is permitted (Pony uses primes on parameter names).
    """
    if name.size() == 0 then return false end
    // Strip trailing prime before validation
    let check = _strip_prime(name)
    if check.size() == 0 then return false end
    var i: USize = 0
    try
      // Skip optional leading underscore
      if check(0)? == '_' then
        i = 1
        if i >= check.size() then return false end
      end
      // First significant character must be uppercase
      let first = check(i)?
      if not is_upper(first) then return false end
      i = i + 1
      // Remaining must be alphanumeric (no underscores)
      while i < check.size() do
        let ch = check(i)?
        if not (is_upper(ch) or is_lower(ch) or is_digit(ch)) then
          return false
        end
        i = i + 1
      end
      true
    else
      false
    end

  fun is_snake_case(name: String val): Bool =>
    """
    Returns true if `name` follows snake_case conventions: starts with a
    lowercase letter (after optional leading `_`), contains only lowercase
    letters, digits, and underscores. A trailing prime (`'`) is permitted
    (Pony uses primes on parameter names).
    """
    if name.size() == 0 then return false end
    // Strip trailing prime before validation
    let check = _strip_prime(name)
    if check.size() == 0 then return false end
    var i: USize = 0
    try
      // Skip optional leading underscore
      if check(0)? == '_' then
        i = 1
        if i >= check.size() then return false end
      end
      // First significant character must be lowercase
      let first = check(i)?
      if not (is_lower(first) or is_digit(first)) then return false end
      i = i + 1
      // Remaining must be lowercase, digit, or underscore
      while i < check.size() do
        let ch = check(i)?
        if not (is_lower(ch) or is_digit(ch) or (ch == '_')) then
          return false
        end
        i = i + 1
      end
      true
    else
      false
    end

  fun has_lowered_acronym(name: String val): (String val | None) =>
    """
    Returns the first known acronym found in CamelCase-lowered form within
    `name`, or None if all acronyms are properly uppercased.

    For example, "JsonDoc" returns "JSON" because "Json" should be "JSON".
    "JSONDoc" returns None because "JSON" is already uppercase.

    Known limitation: a name containing both forms (e.g., "JSONJsonDoc") is
    not flagged because the presence check is not positional.
    """
    let acronyms = _acronyms()
    for acronym in acronyms.values() do
      // Build the lowered form: first char upper, rest lower
      // e.g., "HTTP" -> "Http", "JSON" -> "Json"
      let lowered = _to_title_case(acronym)
      if _contains_substring(name, lowered) then
        // Verify the acronym is NOT already fully uppercased at that position
        if not _contains_substring(name, acronym) then
          return acronym
        end
      end
    end
    None

  fun to_snake_case(name: String val): String val =>
    """
    Convert a CamelCase name to snake_case, handling initialisms correctly.

    The algorithm walks characters left to right:
    - Insert word break before uppercase that follows lowercase
    - Insert word break before last char of uppercase sequence when followed
      by lowercase (to keep initialisms together: "SSLContext" -> "ssl_context")
    - Join words with `_`, lowercase everything

    Leading underscore is preserved if present.
    """
    if name.size() == 0 then return "" end

    let result = recover iso String end
    var i: USize = 0

    try
      // Preserve leading underscore
      if name(0)? == '_' then
        result.push('_')
        i = 1
      end

      while i < name.size() do
        let ch = name(i)?
        if is_upper(ch) and (i > 0) then
          // Check if previous non-underscore char was lowercase
          let prev_idx = i - 1
          let prev = name(prev_idx)?
          if is_lower(prev) or is_digit(prev) then
            // Uppercase after lowercase: new word
            result.push('_')
          elseif is_upper(prev) then
            // In an uppercase sequence: check if next char is lowercase
            if ((i + 1) < name.size()) then
              let next = name(i + 1)?
              if is_lower(next) then
                // Last char of uppercase sequence before lowercase: new word
                // But only if we're not at the very start of significant chars
                let sig_start: USize =
                  if try name(0)? == '_' else false end then 1 else 0 end
                if i > sig_start then
                  result.push('_')
                end
              end
            end
          end
        end
        result.push(_to_lower_ch(ch))
        i = i + 1
      end
    end

    consume result

  fun _acronyms(): Array[String val] val =>
    """Known acronyms that should be fully uppercased in CamelCase names."""
    [
      "HTTP"; "JSON"; "XML"; "URL"; "TCP"; "UDP"; "SSL"; "TLS"
      "FFI"; "AST"; "API"; "HTML"; "CSS"; "DNS"; "SSH"; "URI"
      "UUID"; "MIME"; "SMTP"; "IMAP"; "FIFO"
    ]

  fun _to_title_case(s: String val): String val =>
    """
    Convert an all-uppercase string to title case: first char uppercase,
    rest lowercase. E.g., "HTTP" -> "Http".
    """
    if s.size() == 0 then return "" end
    recover val
      let result = String(s.size())
      var first = true
      for byte in s.values() do
        if first then
          result.push(byte)
          first = false
        else
          result.push(_to_lower_ch(byte))
        end
      end
      result
    end

  fun _contains_substring(haystack: String val, needle: String val): Bool =>
    """Check if haystack contains needle as a substring."""
    if needle.size() > haystack.size() then return false end
    var i: USize = 0
    let limit = (haystack.size() - needle.size()) + 1
    while i < limit do
      if haystack.at(needle, i.isize()) then
        return true
      end
      i = i + 1
    end
    false

  fun _strip_prime(name: String val): String val =>
    """Strip a trailing prime character from the name."""
    if name.size() == 0 then return name end
    try
      if name(name.size() - 1)? == '\'' then
        name.substring(0, (name.size() - 1).isize())
      else
        name
      end
    else
      name
    end

  fun is_upper(ch: U8): Bool =>
    """True if `ch` is an ASCII uppercase letter."""
    (ch >= 'A') and (ch <= 'Z')

  fun is_lower(ch: U8): Bool =>
    """True if `ch` is an ASCII lowercase letter."""
    (ch >= 'a') and (ch <= 'z')

  fun is_digit(ch: U8): Bool =>
    """True if `ch` is an ASCII digit."""
    (ch >= '0') and (ch <= '9')

  fun _to_lower_ch(ch: U8): U8 =>
    if is_upper(ch) then ch + 32 else ch end
