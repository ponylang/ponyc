use "collections"

type IniMap is Map[String, Map[String, String]]
  """
  The nested map produced by `IniParse`: outer keys are section names, inner
  keys are key names, inner values are value strings. The empty-string
  section `""` holds any keys that appear before the first section header.
  """

primitive IniParse
  """
  Parse INI-formatted lines into a nested map of sections to key/value
  pairs.

  Duplicate keys in the same section overwrite previous values; the last
  occurrence wins. Repeated `[section]` headers are merged into the existing
  section.
  """
  fun apply(lines: Iterator[String]): IniMap ref^ ? =>
    """
    Parse `lines` into an `IniMap`. Raises an error on the first malformed
    line.
    """
    let map = IniMap

    let f = object
      let map: IniMap = map

      fun ref apply(section: String, key: String, value: String): Bool =>
        try
          if not map.contains(section) then
            map.insert(section, Map[String, String])
          end
          map(section)?(key) = value
        end
        true

      fun ref add_section(section: String): Bool =>
        if not map.contains(section) then
          map.insert(section, Map[String, String])
        end
        true

      fun ref errors(line: USize, err: IniError): Bool =>
        false
    end

    if not Ini(lines, f) then
      error
    end

    map
