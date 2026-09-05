use "buffered"
use "files"
use "pony_test"
use dep = ".."

class \nodoc\ _TestArchiveDecoderRoundTrip is UnitTest
  fun name(): String => "ArchiveDecoder/round-trip single file"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path
    let dir = Directory(root)?

    let f = dir.create_file("hello.pony")?
    f.print("actor Main")
    f.dispose()

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root.join("hello.pony")?)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let out_path = root.join("output")?
    out_path.mkdir()
    dep.ArchiveDecoder(archive_path, Directory(out_path)?)?

    let extracted = File.open(out_path.join("hello.pony")?)
    let content: String val = extracted.read_string(extracted.size())
    extracted.dispose()
    h.assert_eq[String val](content, "actor Main\n")
    tmp.dispose()

class \nodoc\ _TestArchiveDecoderRoundTripNested is UnitTest
  fun name(): String => "ArchiveDecoder/round-trip nested directories"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path
    let dir = Directory(root)?

    dir.mkdir("pkg")
    dir.mkdir("pkg/sub")
    let f1 = dir.create_file("pkg/top.pony")?
    f1.print("primitive Top")
    f1.dispose()
    let f2 = dir.create_file("pkg/sub/deep.pony")?
    f2.print("primitive Deep")
    f2.dispose()

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root.join("pkg")?)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let out_path = root.join("output")?
    out_path.mkdir()
    dep.ArchiveDecoder(archive_path, Directory(out_path)?)?

    let top = File.open(out_path.join("pkg/top.pony")?)
    let top_content: String val = top.read_string(top.size())
    top.dispose()
    h.assert_eq[String val](top_content, "primitive Top\n")

    let deep = File.open(out_path.join("pkg/sub/deep.pony")?)
    let deep_content: String val = deep.read_string(deep.size())
    deep.dispose()
    h.assert_eq[String val](deep_content, "primitive Deep\n")
    tmp.dispose()

class \nodoc\ _TestArchiveDecoderEmptyDirectory is UnitTest
  fun name(): String => "ArchiveDecoder/round-trip empty directory"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path
    Directory(root)?.mkdir("empty")

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root.join("empty")?)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let out_path = root.join("output")?
    out_path.mkdir()
    dep.ArchiveDecoder(archive_path, Directory(out_path)?)?

    let info = FileInfo(out_path.join("empty")?)?
    h.assert_true(info.directory)
    tmp.dispose()

class \nodoc\ _TestArchiveDecoderRoundTripEmptyFile is UnitTest
  fun name(): String => "ArchiveDecoder/round-trip empty file"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let f = Directory(root)?.create_file("empty.pony")?
    f.dispose()

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root.join("empty.pony")?)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let out_path = root.join("output")?
    out_path.mkdir()
    dep.ArchiveDecoder(archive_path, Directory(out_path)?)?

    let extracted = File.open(out_path.join("empty.pony")?)
    h.assert_eq[USize](extracted.size(), 0)
    extracted.dispose()
    tmp.dispose()

class \nodoc\ _TestArchiveDecoderRejectsTruncatedArchive is UnitTest
  fun name(): String => "ArchiveDecoder/rejects truncated archive"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let writer = Writer
    writer.u8(1)
    writer.u8(1)
    let path: String = "hello.pony"
    writer.u32_le(path.size().u32())
    writer.write(path)
    writer.u32_le(U32(100))
    writer.write("short")

    let archive_path = root.join("bad.par")?
    _WriteRawArchive(archive_path, consume writer)?

    let out_path = root.join("output")?
    out_path.mkdir()

    h.assert_error({() ? =>
      dep.ArchiveDecoder(archive_path, Directory(out_path)?)?
    })
    tmp.dispose()

class \nodoc\ _TestArchiveDecoderErrorsOnMkdirFailure is UnitTest
  fun name(): String => "ArchiveDecoder/errors on mkdir failure"

  fun apply(h: TestHelper) ? =>
    ifdef not windows then
      let tmp = _TestHelper.tmp_dir(h)?
      let root = tmp.path

      let writer = Writer
      writer.u8(1)
      writer.u8(2)
      let dir_path: String = "subdir"
      writer.u32_le(dir_path.size().u32())
      writer.write(dir_path)

      let archive_path = root.join("test.par")?
      _WriteRawArchive(archive_path, consume writer)?

      let out_path = root.join("output")?
      out_path.mkdir()
      @chmod(out_path.path.cstring(), U32(0x124))

      h.assert_error({() ? =>
        dep.ArchiveDecoder(archive_path, Directory(out_path)?)?
      })

      @chmod(out_path.path.cstring(), U32(0x1C0))
      tmp.dispose()
    end

class \nodoc\ _TestArchiveDecoderErrorsOnMissingArchive is UnitTest
  fun name(): String => "ArchiveDecoder/errors on missing archive"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let archive_path = root.join("nonexistent.par")?
    let out_path = root.join("output")?
    out_path.mkdir()

    h.assert_error({() ? =>
      dep.ArchiveDecoder(archive_path, Directory(out_path)?)?
    })
    tmp.dispose()

class \nodoc\ _TestArchiveDecoderRejectsPathTraversal is UnitTest
  fun name(): String => "ArchiveDecoder/rejects path traversal"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let writer = Writer
    writer.u8(1)
    writer.u8(2)
    let dir_path: String = "sub"
    writer.u32_le(dir_path.size().u32())
    writer.write(dir_path)
    writer.u8(1)
    let bad_path: String = "sub/../escape.pony"
    writer.u32_le(bad_path.size().u32())
    writer.write(bad_path)
    writer.u32_le(U32(4))
    writer.write("evil")

    let archive_path = root.join("bad.par")?
    _WriteRawArchive(archive_path, consume writer)?

    let out_path = root.join("output")?
    out_path.mkdir()

    h.assert_error({() ? =>
      dep.ArchiveDecoder(archive_path, Directory(out_path)?)?
    })
    tmp.dispose()

class \nodoc\ _TestArchiveDecoderRejectsUnknownVersion is UnitTest
  fun name(): String => "ArchiveDecoder/rejects unknown version"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let writer = Writer
    writer.u8(99)

    let archive_path = root.join("bad.par")?
    _WriteRawArchive(archive_path, consume writer)?

    let out_path = root.join("output")?
    out_path.mkdir()

    h.assert_error({() ? =>
      dep.ArchiveDecoder(archive_path, Directory(out_path)?)?
    })
    tmp.dispose()

class \nodoc\ _TestArchiveDecoderRejectsUnknownType is UnitTest
  fun name(): String => "ArchiveDecoder/rejects unknown entry type"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let writer = Writer
    writer.u8(1)
    writer.u8(99)
    let path: String = "foo.pony"
    writer.u32_le(path.size().u32())
    writer.write(path)

    let archive_path = root.join("bad.par")?
    _WriteRawArchive(archive_path, consume writer)?

    let out_path = root.join("output")?
    out_path.mkdir()

    h.assert_error({() ? =>
      dep.ArchiveDecoder(archive_path, Directory(out_path)?)?
    })
    tmp.dispose()

class \nodoc\ _TestArchiveDecoderRejectsAbsolutePath is UnitTest
  fun name(): String => "ArchiveDecoder/rejects absolute path"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let writer = Writer
    writer.u8(1)
    writer.u8(2)
    let bad_path: String = "/etc"
    writer.u32_le(bad_path.size().u32())
    writer.write(bad_path)

    let archive_path = root.join("bad.par")?
    _WriteRawArchive(archive_path, consume writer)?

    let out_path = root.join("output")?
    out_path.mkdir()

    h.assert_error({() ? =>
      dep.ArchiveDecoder(archive_path, Directory(out_path)?)?
    })
    tmp.dispose()

primitive \nodoc\ _WriteRawArchive
  fun apply(path: FilePath, writer: Writer iso) ? =>
    match CreateFile(path)
    | let f: File =>
      for chunk in (consume writer).done().values() do
        f.write(chunk)
      end
      f.dispose()
    else
      error
    end
