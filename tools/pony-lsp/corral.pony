use "files"
use "collections"

class val Locator is (Hashable & Stringable)
  """
  Encapsulation of the bundle dependency's locator string, parsed into distinct
  fields.
    locator := repo_path[.vcs_suffix][/bundle_path]
  """
  let repo_path: String
  let vcs_suffix: String
  let bundle_path: String

  new val create(loc: String) =>
    let suffixes = [".git"; ".hg"; ".bzr"; ".svn"]
    var rp = ""
    var vs = ""
    var bp = ""
    for s in suffixes.values() do
      let parts = loc.split_by(s)
      if parts.size() == 2 then try
        rp = parts(0)?
        vs = s
        let p = parts(1)?
        bp = if p.at("/") then p.trim(1) else p end
        break
      end end
    end
    if vs == "" then
      bp = loc  // With no vcs, the locator is the local bundle path
    end
    repo_path = rp
    vcs_suffix = vs
    bundle_path = bp

  fun path(): String =>
    """Returns a unique name for this locator without the vcs suffix."""
    Path.to_slash(Path.join(repo_path, bundle_path))

  fun flat_name(): String =>
    _Flattened(path())

  fun string(): String iso^ =>
    """Returns the full string for of this locator."""
    Path.to_slash(Path.join(repo_path + vcs_suffix, bundle_path)).clone()

  fun compare(that: Locator box): Compare =>
    if (repo_path != that.repo_path) then return repo_path.compare(that.repo_path) end
    if (vcs_suffix != that.vcs_suffix) then return vcs_suffix.compare(that.vcs_suffix) end
    bundle_path.compare(that.bundle_path)

  fun hash(): USize =>
    repo_path.hash() xor vcs_suffix.hash() xor bundle_path.hash()

  fun is_vcs(): Bool => vcs_suffix != ""

  fun is_local(): Bool => (repo_path == "") or repo_path.at(".") or repo_path.at("/")

  fun is_remote_vcs(): Bool => is_vcs() and not is_local()

  fun is_local_vcs(): Bool => is_vcs() and is_local()

  fun is_local_direct(): Bool => not is_vcs() and is_local()

primitive _Flattened
  fun apply(path: String): String val =>
    let dash_code: U8 = '_'
    let path_name_arr = recover val
      var acc: Array[U8] = Array[U8]
      for char in path.array().values() do
        if _is_alphanum(char) then
          acc.push(char)
        else
          try // we know we don't index out of bounds
            if acc.size() == 0 then
              acc.push(dash_code)
            elseif acc(acc.size() - 1)? != dash_code then
              acc.push(dash_code)
            end
          end
        end
      end
      consume acc
    end
    String.from_array(consume path_name_arr)

  fun _is_alphanum(c: U8): Bool =>
    (('0' <= c) and (c <= '9'))
      or
    (('A' <= c) and (c <= 'Z'))
      or
    (('a' <= c) and (c <= 'z'))
      or
    (c == '_')
