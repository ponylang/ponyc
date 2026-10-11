// use "debug"
use @module_init[Bool](opt: _PassOpt)
use @module_add_paths[None](
  paths: Pointer[U8] tag, opy: _PassOpt)
use @package_init_lib[Bool](
  opt: _PassOpt,
  pony_installation: Pointer[U8] tag)
use @module_done[None](opt: _PassOpt)

class val Module
  """
  Represents a pony module.
  """
  let ast: AST val
  let hygienic_id: String val
  let qualified_name: String val
  let path: String val
  // kept so the underlying AST is not lost; reaped when Program is GC'd
  let _program: Program val

  new val create(program: Program val, ast': AST) ? =>
    _program = program
    ast = ast'
    let module_data: _Package = ast.module_data().apply()?
    qualified_name = module_data.qualified_name()
    path = module_data.path()
    hygienic_id = module_data.hygienic_id()

  fun val file(): (SourceFile val | None) =>
    """
    Returns the first SourceFile in this Module.
    """
    match ast.child()
    | let m_ast: AST =>
      try
        SourceFile.create(this._program, m_ast)?
      end
    end

  fun val files(): Iterator[SourceFile val] =>
    _SourceFileIter.create(_program, this)

  fun val find_file(path': String): (SourceFile val | None) =>
    """
    Find a file in this module by file path.
    """
    // Debug("trying to find file from: " + path')
    for f in files() do
      // Debug("checking: " +  f.file)
      if f.file == path' then
        // Debug("found file: " + path')
        return f
      end
    end

class _ModuleIter is Iterator[Module]
  var _module_ast: (AST val | None)
  let _program: Program val

  new ref create(program: Program val) =>
    _module_ast = program.ast.child()
    _program = program

  fun ref has_next(): Bool =>
    _module_ast isnt None

  fun ref next(): Module val ? =>
    let module_ast = _module_ast as AST
    _module_ast = module_ast.sibling()
    Module.create(_program, module_ast)?

primitive _PackageSet
  """
  STUB
  """

primitive _PackageGroup
  """
  STUB
  """

struct _Package
  let _path: Pointer[U8] val = _path.create()
    """
    Absolute path.
    """
  let _qualified_name: Pointer[U8] val =
    _qualified_name.create()
    """
    For pretty printing, eg "builtin".
    """
  let _id: Pointer[U8] val = _id.create()
    """
    Hygienic identifier.
    """
  let _filename: Pointer[U8] val = _filename.create()
    """
    Directory name.
    """
  let symbol: Pointer[U8] val = symbol.create()
    """
    Wart to use for symbol names.
    """
  let ast: Pointer[_AST] = ast.create()
  let dependencies: Pointer[_PackageSet] =
    dependencies.create()
  let group: Pointer[_PackageGroup] = group.create()
  let group_index: USize = 0
  let next_hygienic_id: USize = 0
  let low_index: USize = 0
  let allow_ffi: Bool = true
  let on_stack: Bool = true

  fun qualified_name(): String val =>
    recover val
      String.copy_cstring(this._qualified_name)
    end

  fun path(): String val =>
    recover val String.copy_cstring(this._path) end

  fun hygienic_id(): String val =>
    recover val String.copy_cstring(this._id) end

  fun filename(): String val =>
    recover val
      String.copy_cstring(this._filename)
    end
