use "files"
use "pony_test"
use dep = ".."

class \nodoc\ _TestArchiveEncoderSingleFile is UnitTest
  fun name(): String => "ArchiveEncoder/single file"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let f = Directory(root)?.create_file("hello.pony")?
    f.print("actor Main")
    f.dispose()

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root.join("hello.pony")?)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let reader = _TestHelper.read_archive(archive_path)?
    h.assert_eq[U8](reader.u8()?, 1)

    h.assert_eq[U8](reader.u8()?, 1)
    let path_size = reader.u32_le()?.usize()
    let path = String.from_array(reader.block(path_size)?)
    h.assert_eq[String](path, "hello.pony")

    let content_size = reader.u32_le()?.usize()
    let content = String.from_array(reader.block(content_size)?)
    h.assert_true(content.contains("actor Main"))

    h.assert_eq[USize](reader.size(), 0)
    tmp.dispose()

class \nodoc\ _TestArchiveEncoderDirectory is UnitTest
  fun name(): String => "ArchiveEncoder/directory with files"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path
    let dir = Directory(root)?

    dir.mkdir("src")
    let f1 = dir.create_file("src/main.pony")?
    f1.print("actor Main")
    f1.dispose()
    let f2 = dir.create_file("src/helper.pony")?
    f2.print("class Helper")
    f2.dispose()

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root.join("src")?)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let reader = _TestHelper.read_archive(archive_path)?
    h.assert_eq[U8](reader.u8()?, 1)

    // directory entry for "src"
    h.assert_eq[U8](reader.u8()?, 2)
    var ps = reader.u32_le()?.usize()
    var p = String.from_array(reader.block(ps)?)
    h.assert_eq[String](p, "src")

    h.assert_eq[U8](reader.u8()?, 1)
    ps = reader.u32_le()?.usize()
    p = String.from_array(reader.block(ps)?)
    h.assert_eq[String](p, "src/helper.pony")
    var cs = reader.u32_le()?.usize()
    reader.skip(cs)?

    h.assert_eq[U8](reader.u8()?, 1)
    ps = reader.u32_le()?.usize()
    p = String.from_array(reader.block(ps)?)
    h.assert_eq[String](p, "src/main.pony")
    cs = reader.u32_le()?.usize()
    reader.skip(cs)?

    h.assert_eq[USize](reader.size(), 0)
    tmp.dispose()

class \nodoc\ _TestArchiveEncoderNestedDirectories is UnitTest
  fun name(): String => "ArchiveEncoder/nested directories"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path
    let dir = Directory(root)?

    dir.mkdir("pkg")
    dir.mkdir("pkg/sub")
    let f = dir.create_file("pkg/sub/deep.pony")?
    f.print("primitive Deep")
    f.dispose()

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root.join("pkg")?)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let reader = _TestHelper.read_archive(archive_path)?
    h.assert_eq[U8](reader.u8()?, 1)

    // directory: pkg
    h.assert_eq[U8](reader.u8()?, 2)
    var ps = reader.u32_le()?.usize()
    var p = String.from_array(reader.block(ps)?)
    h.assert_eq[String](p, "pkg")

    // directory: pkg/sub
    h.assert_eq[U8](reader.u8()?, 2)
    ps = reader.u32_le()?.usize()
    p = String.from_array(reader.block(ps)?)
    h.assert_eq[String](p, "pkg/sub")

    // file: pkg/sub/deep.pony
    h.assert_eq[U8](reader.u8()?, 1)
    ps = reader.u32_le()?.usize()
    p = String.from_array(reader.block(ps)?)
    h.assert_eq[String](p, "pkg/sub/deep.pony")
    tmp.dispose()

class \nodoc\ _TestArchiveEncoderEmptyDirectory is UnitTest
  fun name(): String => "ArchiveEncoder/empty directory"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path
    Directory(root)?.mkdir("empty")

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root.join("empty")?)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let reader = _TestHelper.read_archive(archive_path)?
    h.assert_eq[U8](reader.u8()?, 1)

    // directory entry for "empty", nothing else
    h.assert_eq[U8](reader.u8()?, 2)
    let ps = reader.u32_le()?.usize()
    let p = String.from_array(reader.block(ps)?)
    h.assert_eq[String](p, "empty")

    h.assert_eq[USize](reader.size(), 0)
    tmp.dispose()

class \nodoc\ _TestArchiveEncoderSkipsSymlinks is UnitTest
  fun name(): String => "ArchiveEncoder/skips symlinks"

  fun apply(h: TestHelper) ? =>
    ifdef not windows then
      let tmp = _TestHelper.tmp_dir(h)?
      let root = tmp.path
      let dir = Directory(root)?

      let f = dir.create_file("real.pony")?
      f.print("primitive Real")
      f.dispose()

      @symlink(
        "real.pony".cstring(),
        root.join("linked.pony")?.path.cstring())

      let encoder = dep.ArchiveEncoder(root)?
      encoder.add(root.join("real.pony")?)?
      encoder.add(root.join("linked.pony")?)?

      let archive_path = root.join("test.par")?
      encoder.write(archive_path)?

      let reader = _TestHelper.read_archive(archive_path)?
      h.assert_eq[U8](reader.u8()?, 1)

      // only real.pony, no linked.pony
      h.assert_eq[U8](reader.u8()?, 1)
      let ps = reader.u32_le()?.usize()
      let p = String.from_array(reader.block(ps)?)
      h.assert_eq[String](p, "real.pony")
      let cs = reader.u32_le()?.usize()
      reader.skip(cs)?

      h.assert_eq[USize](reader.size(), 0)
      tmp.dispose()
    end

class \nodoc\ _TestArchiveEncoderDeterministicOrder is UnitTest
  fun name(): String => "ArchiveEncoder/deterministic ordering"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path
    let dir = Directory(root)?

    dir.mkdir("pkg")
    let f3 = dir.create_file("pkg/zebra.pony")?
    f3.print("primitive Zebra")
    f3.dispose()
    let f1 = dir.create_file("pkg/alpha.pony")?
    f1.print("primitive Alpha")
    f1.dispose()
    let f2 = dir.create_file("pkg/middle.pony")?
    f2.print("primitive Middle")
    f2.dispose()

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root.join("pkg")?)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let reader = _TestHelper.read_archive(archive_path)?
    h.assert_eq[U8](reader.u8()?, 1)

    // directory
    h.assert_eq[U8](reader.u8()?, 2)
    var ps = reader.u32_le()?.usize()
    reader.skip(ps)?

    // files in alphabetical order
    let expected = ["alpha.pony"; "middle.pony"; "zebra.pony"]
    for name' in expected.values() do
      h.assert_eq[U8](reader.u8()?, 1)
      ps = reader.u32_le()?.usize()
      let p = String.from_array(reader.block(ps)?)
      h.assert_eq[String](p, "pkg/" + name')
      let cs = reader.u32_le()?.usize()
      reader.skip(cs)?
    end
    tmp.dispose()

class \nodoc\ _TestArchiveEncoderRootDirectory is UnitTest
  fun name(): String => "ArchiveEncoder/root directory skips dot entry"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let f = Directory(root)?.create_file("main.pony")?
    f.print("actor Main")
    f.dispose()

    let encoder = dep.ArchiveEncoder(root)?
    encoder.add(root)?

    let archive_path = root.join("test.par")?
    encoder.write(archive_path)?

    let reader = _TestHelper.read_archive(archive_path)?
    h.assert_eq[U8](reader.u8()?, 1)

    h.assert_eq[U8](reader.u8()?, 1)
    let ps = reader.u32_le()?.usize()
    let p = String.from_array(reader.block(ps)?)
    h.assert_eq[String](p, "main.pony")
    let cs = reader.u32_le()?.usize()
    reader.skip(cs)?

    h.assert_eq[USize](reader.size(), 0)
    tmp.dispose()

class \nodoc\ _TestArchiveEncoderUnreadableFile is UnitTest
  fun name(): String => "ArchiveEncoder/errors on unreadable file"

  fun apply(h: TestHelper) ? =>
    ifdef not windows then
      let tmp = _TestHelper.tmp_dir(h)?
      let root = tmp.path
      let dir = Directory(root)?

      let f = dir.create_file("nope.pony")?
      f.print("secret")
      f.dispose()

      let file_path = root.join("nope.pony")?
      @chmod(file_path.path.cstring(), U32(0))

      h.assert_error({() ? =>
        let encoder = dep.ArchiveEncoder(root)?
        encoder.add(file_path)?
      })

      @chmod(file_path.path.cstring(), U32(0x180))
      tmp.dispose()
    end
