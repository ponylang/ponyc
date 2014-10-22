class Directory val
  let path: String
  let files: Array[String] val

  new create(from: String) ? =>
    path = from
    files = recover
      var list = Array[String]

      @os_opendir[None](from.cstring()) ?

      while true do
        var entry = @os_readdir[Pointer[U8] iso^]()

        if entry.u64() == 0 then
          break None
        end

        list.append(recover String.from_cstring(consume entry) end)
      end

      @os_closedir[None]()
      consume list
    end
