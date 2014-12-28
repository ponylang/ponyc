class Directory val
  """
  The entries will include everything in the directory. The path for the entry
  will be relative to the directory, so it will contain no directory separators.
  The entries will not include "." or "..".
  """
  let path: String
  let entries: Array[String] val

  new create(from: String) ? =>
    """
    This will raise an error if the path doesn't exist or it is not a directory.
    """
    path = from
    entries = recover
      let list = Array[String]

      @os_opendir[None](from.cstring()) ?

      while true do
        let entry = @os_readdir[Pointer[U8] iso^]()

        if entry.is_null() then
          break
        end

        list.append(recover String.from_cstring(consume entry) end)
      end

      @os_closedir[None]()
      consume list
    end
