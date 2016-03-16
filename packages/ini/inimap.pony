use "collections"

type IniMap is Map[String, Map[String, String]]

primitive IniParse
  """
  This is used to parse INI formatted text as a nested map of strings.
  """
  fun apply(lines: Iterator[String]): IniMap ref^ ? =>
    """
    This accepts a string iterator and returns a nested map of strings. If
    parsing fails, an error is raised.
    """
    let map = IniMap

    let f = object
      let map: IniMap = map

      fun ref apply(section: String, key: String, value: String): Bool =>
        try
          (try
            map(section)
          else
            map.insert(section, Map[String, String])
          end)(key) = value
        end
        true

      fun ref errors(line: USize, err: IniError): Bool =>
        false
    end

    if not Ini(lines, f) then
      error
    end

    map
