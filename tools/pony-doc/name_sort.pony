primitive NameSort
  """
  Comparator for sorting names alphabetically, ignoring a single
  leading underscore. Matches docgen.c's `doc_list_cmp()`.
  """
  fun compare(a: String box, b: String box): Compare =>
    _stripped(a).compare(_stripped(b))

  fun _stripped(name: String box): String val =>
    if (name.size() > 0) and (try name(0)? == '_' else false end) then
      name.substring(1)
    else
      name.clone()
    end
