use "buffered"
use "files"

primitive ArchiveDecoder
  """
  Decodes a Pony archive (.par) into a target directory.

  Rejects invalid paths, unknown version bytes, and unknown entry types.
  Fails hard on invalid paths rather than skipping them.
  """
  fun apply(archive: FilePath, to: Directory) ? =>
    let reader: Reader = Reader

    match OpenFile(archive)
    | let f: File =>
      reader.append(f.read(f.size()))
      f.dispose()

      let version = reader.u8()?
      if version != 1 then
        error
      end

      while reader.size() > 0 do
        let entry_type = reader.u8()?
        match entry_type
        | 1 =>
          let path_size = reader.u32_le()?.usize()
          let path_block = reader.block(path_size)?
          let path = String.from_array(consume path_block)

          if not PathValidator(path) then
            error
          end

          let content_size = reader.u32_le()?.usize()
          let content = reader.block(content_size)?

          let file = to.create_file(path)?
          if not file.set_length(0) then
            file.dispose()
            error
          end
          if not file.write(consume content) then
            file.dispose()
            error
          end
          file.dispose()
        | 2 =>
          let path_size = reader.u32_le()?.usize()
          let path_block = reader.block(path_size)?
          let path = String.from_array(consume path_block)

          if not PathValidator(path) then
            error
          end

          if not to.mkdir(path) then
            error
          end
        else
          error
        end
      end
    else
      error
    end
