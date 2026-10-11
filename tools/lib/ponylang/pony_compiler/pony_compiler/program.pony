use "lib:ponyc-standalone"
use "lib:z" if not windows
use "lib:c++" if osx
use "files"


use @program_create[_Program]()
use @program_free[None](program: _Program)
use @program_load[Pointer[_AST] val](
  path: Pointer[U8] tag, opt: _PassOpt)

use @printf[None](s: Pointer[U8] tag, ...)

struct _Program

type VerbosityLevel is
  ( VerbosityQuiet
  | VerbosityMinimal
  | VerbosityInfo
  | VerbosityToolInfo
  | VerbosityAll )

primitive VerbosityQuiet
  """
  No output.
  """
  fun apply(): I32 => 0

primitive VerbosityMinimal
  """
  Minimal output.
  """
  fun apply(): I32 => 1

primitive VerbosityInfo
  """
  Informational output.
  """
  fun apply(): I32 => 2

primitive VerbosityToolInfo
  """
  Tool-level informational output.
  """
  fun apply(): I32 => 3

primitive VerbosityAll
  """
  All output.
  """
  fun apply(): I32 => 4

class val Program
  """
  A compiled Pony program, wrapping the underlying AST.
  """
  let ast: AST

  new val create(ast': AST) =>
    ast = ast'

  fun val module(): (Module | None) =>
    """
    The module representing the source directory.
    """
    match ast.child()
    | let p_ast: AST =>
      try
        Module.create(this, p_ast)?
      end
    end

  fun val modules(): Iterator[Module] =>
    """
    Source directory is always the first module.
    Second is builtin.
    The rest is still unknown to me.
    """
    _ModuleIter.create(this)

  fun val apply(module': String): Module ? =>
    let module_ast =
      ast.find_in_scope(module') as AST
    Module.create(this, module_ast)?

  fun _final() =>
    // The Program owns both the AST and its
    // interned-string table. The table is detached from
    // the pass_opt at construction time (so
    // pass_opt_done does not free it while the AST is
    // still in use) and freed here, once the AST that
    // references its interned strings is gone.
    if not ast.raw.is_null() then
      @ast_free(ast.raw)
    end
    if not ast.strtab.is_null() then
      @stringtab_free(ast.strtab)
    end
