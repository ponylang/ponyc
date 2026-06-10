use @stringtab_len[Pointer[U8] ref](table: Pointer[_StrTable] tag,
  string: Pointer[U8] tag, len: USize)

// Free an interned-string table. Takes the table pointer by value, so it is
// callable from a box receiver (e.g. Program._final), unlike pass_opt_done
// which needs a ref _PassOpt.
use @stringtab_free[None](table: Pointer[_StrTable] tag)


primitive StringTab
  """
  Interns a string into a specific compilation's interned-string table.

  libponyc no longer keeps a process-global table: each compilation owns its
  table (pass_opt.strtab), so interning a query name for an AST lookup must use
  that same table for the resulting pointer to match the names already interned
  in the AST. Pass the `strtab` carried by the `AST` you are querying.
  """
  fun apply(table: Pointer[_StrTable] tag, string: String box): String val =>
    """
    Interns the given string into the given table and returns a reference to the
    stored copy. The returned string lives as long as the table does (until its
    pass_opt is disposed).
    """
    let len = string.size()
    let str_ptr = string.cpointer()
    recover
      let ptr = @stringtab_len(table, str_ptr, len)
      String.from_cpointer(ptr, len)
    end
