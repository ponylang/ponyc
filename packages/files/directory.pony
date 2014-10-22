class Directory
  var files: Array[String] val

  new create(path: String) ? =>
    files = recover
      var list = Array[String]

      @os_opendir[None](path.cstring()) ?

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
