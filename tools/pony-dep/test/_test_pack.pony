use "files"
use "pony_test"
use dep = ".."

class \nodoc\ _TestPackRoundTrip is UnitTest
  """
  Pack a directory, decode the archive, and verify the contents match.
  """
  fun name(): String => "Pack/round trip"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path
    let dir = Directory(root)?

    let f1 = dir.create_file("main.pony")?
    f1.print("actor Main")
    f1.dispose()
    dir.mkdir("sub")
    let f2 = dir.create_file("sub/helper.pony")?
    f2.print("class Helper")
    f2.dispose()

    let out_tmp = _TestHelper.tmp_dir(h)?
    let archive_path = out_tmp.path.join("out.par")?
    let auth = FileAuth(h.env.root)
    match dep.Pack(auth, root.path, archive_path.path where hash = false)
    | let e: dep.PackError => h.fail("pack failed"); return
    end

    let out_dir = root.join("out")?
    out_dir.mkdir()
    let out = Directory(out_dir)?
    dep.ArchiveDecoder(archive_path, out)?

    let main_file =
      match OpenFile(out_dir.join("main.pony")?)
      | let f: File =>
        let data = f.read(f.size())
        f.dispose()
        String.from_iso_array(consume data)
      else
        h.fail("missing main.pony")
        return
      end
    h.assert_true(main_file.contains("actor Main"))

    let helper_file =
      match OpenFile(out_dir.join("sub/helper.pony")?)
      | let f: File =>
        let data = f.read(f.size())
        f.dispose()
        String.from_iso_array(consume data)
      else
        h.fail("missing sub/helper.pony")
        return
      end
    h.assert_true(helper_file.contains("class Helper"))

    tmp.dispose()
    out_tmp.dispose()

class \nodoc\ _TestPackHash is UnitTest
  """
  Pack with hash=true returns the content hash, matching an independent
  ContentHash computation on the same source.
  """
  fun name(): String => "Pack/hash output"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let f = Directory(root)?.create_file("hello.pony")?
    f.print("actor Main")
    f.dispose()

    let out_tmp = _TestHelper.tmp_dir(h)?
    let archive_path = out_tmp.path.join("out.par")?
    let auth = FileAuth(h.env.root)
    let result = dep.Pack(auth, root.path, archive_path.path
      where hash = true)

    match result
    | let s: String val =>
      let expected = dep.ContentHash(root)?
      let expected_str: String val = "sha256:" + dep.Sha256.hex(expected)
      h.assert_true(s == expected_str)
    | let e: dep.PackError =>
      h.fail("pack failed")
    else
      h.fail("expected hash string")
    end

    tmp.dispose()
    out_tmp.dispose()

class \nodoc\ _TestPackEmptyDirectory is UnitTest
  """
  Pack an empty directory — produces a valid archive with only the version
  byte.
  """
  fun name(): String => "Pack/empty directory"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let out_tmp = _TestHelper.tmp_dir(h)?
    let archive_path = out_tmp.path.join("out.par")?
    let auth = FileAuth(h.env.root)
    match dep.Pack(auth, root.path, archive_path.path where hash = false)
    | let e: dep.PackError => h.fail("pack failed"); return
    end

    let reader = _TestHelper.read_archive(archive_path)?
    h.assert_eq[U8](reader.u8()?, 1)
    h.assert_eq[USize](reader.size(), 0)
    tmp.dispose()
    out_tmp.dispose()

class \nodoc\ _TestPackSourceNotFound is UnitTest
  """
  Pack returns PackSourceNotFound for a nonexistent source path.
  """
  fun name(): String => "Pack/source not found"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let out_tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let source = tmp.path.join("nonexistent")?.path
    let output = out_tmp.path.join("out.par")?.path
    match dep.Pack(auth, source, output where hash = false)
    | dep.PackSourceNotFound => None
    else
      h.fail("expected PackSourceNotFound")
    end
    tmp.dispose()
    out_tmp.dispose()

class \nodoc\ _TestPackSourceIsFile is UnitTest
  """
  Pack returns PackSourceNotDirectory when the source is a regular file.
  """
  fun name(): String => "Pack/source is a file"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path

    let f = Directory(root)?.create_file("not-a-dir.txt")?
    f.print("hello")
    f.dispose()

    let out_tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let source = root.join("not-a-dir.txt")?.path
    let output = out_tmp.path.join("out.par")?.path
    match dep.Pack(auth, source, output where hash = false)
    | dep.PackSourceNotDirectory => None
    else
      h.fail("expected PackSourceNotDirectory")
    end
    tmp.dispose()
    out_tmp.dispose()

class \nodoc\ _TestPackOutputInsideSource is UnitTest
  """
  Pack rejects an output path inside the source directory.
  """
  fun name(): String => "Pack/output inside source"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = tmp.path
    let auth = FileAuth(h.env.root)
    let output = root.join("out.par")?.path
    match dep.Pack(auth, root.path, output where hash = false)
    | dep.PackOutputInsideSource => None
    else
      h.fail("expected PackOutputInsideSource")
    end
    tmp.dispose()
