use "files"
use "pony_test"

use dep = ".."

primitive \nodoc\ _ResolveTestHelper
  fun write_config(auth: FileAuth, dir: String, content: String): String ? =>
    let config_path = Path.join(dir, "pony.deps")
    let fp = FilePath(auth, config_path)
    let f = CreateFile(fp) as File
    f.print(content)
    f.dispose()
    config_path

  fun place_dep(
    auth: FileAuth,
    out_dir: String,
    name: String,
    hash: String,
    config_content: (String | None) = None)
    : String ?
  =>
    let dir_name: String val = name + "@" + hash
    let dep_dir = Path.join(out_dir, dir_name)
    FilePath(auth, dep_dir).mkdir()
    match config_content
    | let c: String =>
      write_config(auth, dep_dir, c)?
    end
    dep_dir

class \nodoc\ _TestResolveNoDeps is UnitTest
  fun name(): String => "ResolveDeps completes with no deps"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let out_dir = Path.join(tmp.path.path, "out")
    let config_path = _ResolveTestHelper.write_config(
      auth, tmp.path.path, "version 1\n")?
    dep.ResolveDeps(
      h.env,
      config_path,
      _TestResolveNotify(h),
      out_dir)

class \nodoc\ _TestResolveNoTransitiveDeps is UnitTest
  fun name(): String =>
    "ResolveDeps skips placed deps without pony.deps"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let out_dir = Path.join(tmp.path.path, "out")
    FilePath(auth, out_dir).mkdir()

    let hash_a =
      "e3b0c44298fc1c149afbf4c8996fb924" +
      "27ae41e4649b934ca495991b7852b855"

    _ResolveTestHelper.place_dep(auth, out_dir, "foo", hash_a)?

    let config_path = _ResolveTestHelper.write_config(
      auth, tmp.path.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type par\n" +
      "  url https://example.com/foo.par\n" +
      "  hash sha256:" + hash_a + "\n" +
      "end\n")?

    dep.ResolveDeps(
      h.env,
      config_path,
      _TestResolveNotify(h where expected_skipped = 1),
      out_dir)

class \nodoc\ _TestResolveOneTransitiveLevel is UnitTest
  fun name(): String =>
    "ResolveDeps resolves one level of transitive deps"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let out_dir = Path.join(tmp.path.path, "out")
    FilePath(auth, out_dir).mkdir()

    let hash_a =
      "e3b0c44298fc1c149afbf4c8996fb924" +
      "27ae41e4649b934ca495991b7852b855"
    let hash_b =
      "ca978112ca1bbdcafac231b39a23dc4d" +
      "a786eff8147c4e72b9807785afee48bb"

    _ResolveTestHelper.place_dep(auth, out_dir, "bar", hash_b)?

    _ResolveTestHelper.place_dep(auth, out_dir, "foo", hash_a,
      "version 1\n" +
      "\n" +
      "dep bar\n" +
      "  type par\n" +
      "  url https://example.com/bar.par\n" +
      "  hash sha256:" + hash_b + "\n" +
      "end\n")?

    let config_path = _ResolveTestHelper.write_config(
      auth, tmp.path.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type par\n" +
      "  url https://example.com/foo.par\n" +
      "  hash sha256:" + hash_a + "\n" +
      "end\n")?

    dep.ResolveDeps(
      h.env,
      config_path,
      _TestResolveNotify(h where expected_skipped = 2),
      out_dir)

class \nodoc\ _TestResolveTwoTransitiveLevels is UnitTest
  fun name(): String =>
    "ResolveDeps resolves two levels of transitive deps"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let out_dir = Path.join(tmp.path.path, "out")
    FilePath(auth, out_dir).mkdir()

    let hash_a =
      "e3b0c44298fc1c149afbf4c8996fb924" +
      "27ae41e4649b934ca495991b7852b855"
    let hash_b =
      "ca978112ca1bbdcafac231b39a23dc4d" +
      "a786eff8147c4e72b9807785afee48bb"
    let hash_c =
      "3e23e8160039594a33894f6564e1b134" +
      "8bbd7a0088d42c4acb73eeaed59c009d"

    _ResolveTestHelper.place_dep(auth, out_dir, "baz", hash_c)?

    _ResolveTestHelper.place_dep(auth, out_dir, "bar", hash_b,
      "version 1\n" +
      "\n" +
      "dep baz\n" +
      "  type par\n" +
      "  url https://example.com/baz.par\n" +
      "  hash sha256:" + hash_c + "\n" +
      "end\n")?

    _ResolveTestHelper.place_dep(auth, out_dir, "foo", hash_a,
      "version 1\n" +
      "\n" +
      "dep bar\n" +
      "  type par\n" +
      "  url https://example.com/bar.par\n" +
      "  hash sha256:" + hash_b + "\n" +
      "end\n")?

    let config_path = _ResolveTestHelper.write_config(
      auth, tmp.path.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type par\n" +
      "  url https://example.com/foo.par\n" +
      "  hash sha256:" + hash_a + "\n" +
      "end\n")?

    dep.ResolveDeps(
      h.env,
      config_path,
      _TestResolveNotify(h where expected_skipped = 3),
      out_dir)

class \nodoc\ _TestResolveDiamondDeps is UnitTest
  fun name(): String =>
    "ResolveDeps handles diamond dependencies"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let out_dir = Path.join(tmp.path.path, "out")
    FilePath(auth, out_dir).mkdir()

    let hash_a =
      "e3b0c44298fc1c149afbf4c8996fb924" +
      "27ae41e4649b934ca495991b7852b855"
    let hash_b =
      "ca978112ca1bbdcafac231b39a23dc4d" +
      "a786eff8147c4e72b9807785afee48bb"
    let hash_c =
      "3e23e8160039594a33894f6564e1b134" +
      "8bbd7a0088d42c4acb73eeaed59c009d"

    let shared_dep_config: String val =
      "version 1\n" +
      "\n" +
      "dep baz\n" +
      "  type par\n" +
      "  url https://example.com/baz.par\n" +
      "  hash sha256:" + hash_c + "\n" +
      "end\n"

    _ResolveTestHelper.place_dep(
      auth, out_dir, "baz", hash_c, "version 1\n")?

    _ResolveTestHelper.place_dep(
      auth, out_dir, "foo", hash_a, shared_dep_config)?
    _ResolveTestHelper.place_dep(
      auth, out_dir, "bar", hash_b, shared_dep_config)?

    let config_path = _ResolveTestHelper.write_config(
      auth, tmp.path.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type par\n" +
      "  url https://example.com/foo.par\n" +
      "  hash sha256:" + hash_a + "\n" +
      "end\n" +
      "\n" +
      "dep bar\n" +
      "  type par\n" +
      "  url https://example.com/bar.par\n" +
      "  hash sha256:" + hash_b + "\n" +
      "end\n")?

    dep.ResolveDeps(
      h.env,
      config_path,
      _TestResolveNotify(h where expected_skipped = 3),
      out_dir)

class \nodoc\ _TestResolveRootConfigNotFound is UnitTest
  fun name(): String =>
    "ResolveDeps fails when root config not found"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let out_dir = Path.join(tmp.path.path, "out")
    dep.ResolveDeps(
      h.env,
      Path.join(tmp.path.path, "nonexistent.deps"),
      _TestResolveNotify(
        h where expect_failed = true,
        expected_message = "config file not found"),
      out_dir)

class \nodoc\ _TestResolveRootConfigParseError is UnitTest
  fun name(): String =>
    "ResolveDeps fails on root config parse error"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let config_path = _ResolveTestHelper.write_config(
      auth, tmp.path.path, "garbage content\n")?
    let out_dir = Path.join(tmp.path.path, "out")
    dep.ResolveDeps(
      h.env,
      config_path,
      _TestResolveNotify(
        h where expect_failed = true,
        expected_message = "expected 'version"),
      out_dir)

class \nodoc\ _TestResolveTransitiveConfigParseError is UnitTest
  fun name(): String =>
    "ResolveDeps records transitive config parse error"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let out_dir = Path.join(tmp.path.path, "out")
    FilePath(auth, out_dir).mkdir()

    let hash_a =
      "e3b0c44298fc1c149afbf4c8996fb924" +
      "27ae41e4649b934ca495991b7852b855"

    _ResolveTestHelper.place_dep(
      auth, out_dir, "foo", hash_a, "garbage content\n")?

    let config_path = _ResolveTestHelper.write_config(
      auth, tmp.path.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type par\n" +
      "  url https://example.com/foo.par\n" +
      "  hash sha256:" + hash_a + "\n" +
      "end\n")?

    dep.ResolveDeps(
      h.env,
      config_path,
      _TestResolveNotify(
        h where expected_skipped = 1, expected_errors = 1,
        expected_error_substring = "expected 'version"),
      out_dir)

class \nodoc\ _TestResolveTransitiveUnsupportedDepType is UnitTest
  fun name(): String =>
    "ResolveDeps records error for unsupported transitive dep type"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let auth = FileAuth(h.env.root)
    let out_dir = Path.join(tmp.path.path, "out")
    FilePath(auth, out_dir).mkdir()

    let hash_a =
      "e3b0c44298fc1c149afbf4c8996fb924" +
      "27ae41e4649b934ca495991b7852b855"
    let hash_b =
      "ca978112ca1bbdcafac231b39a23dc4d" +
      "a786eff8147c4e72b9807785afee48bb"

    _ResolveTestHelper.place_dep(auth, out_dir, "bar", hash_b)?

    _ResolveTestHelper.place_dep(auth, out_dir, "foo", hash_a,
      "version 1\n" +
      "\n" +
      "dep bad\n" +
      "  type git\n" +
      "  url https://example.com/bad\n" +
      "  ref v1.0.0\n" +
      "  hash skip\n" +
      "end\n" +
      "\n" +
      "dep bar\n" +
      "  type par\n" +
      "  url https://example.com/bar.par\n" +
      "  hash sha256:" + hash_b + "\n" +
      "end\n")?

    let config_path = _ResolveTestHelper.write_config(
      auth, tmp.path.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type par\n" +
      "  url https://example.com/foo.par\n" +
      "  hash sha256:" + hash_a + "\n" +
      "end\n")?

    dep.ResolveDeps(
      h.env,
      config_path,
      _TestResolveNotify(
        h where expected_skipped = 2, expected_errors = 1,
        expected_error_substring = "unsupported dep type"),
      out_dir)
