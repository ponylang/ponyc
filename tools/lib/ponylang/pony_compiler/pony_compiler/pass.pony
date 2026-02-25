use @pass_opt_init[None](options: _PassOpt)
use @pass_opt_done[None](options: _PassOpt)
use @ast_passes_program[Bool](program: Pointer[_AST], options: _PassOpt)

type PassId is (
  PassParse      | PassSyntax         | PassSugar        | PassScope  |
  PassImport     | PassNameResolution | PassFlatten      | PassTraits | PassDocs |
  PassRefer      | PassExpr           | PassCompleteness | PassVerify | PassFinaliser |
  PassSerialiser | PassReach          | PassPaint        | PassLLVMIR | PassBitcode |
  PassASM        | PassObj            | PassAll
)

primitive PassParse fun apply(): I32 => 0
primitive PassSyntax fun apply(): I32 => 1
primitive PassSugar fun apply(): I32 => 2
primitive PassScope fun apply(): I32 => 3
primitive PassImport fun apply(): I32 => 4
primitive PassNameResolution fun apply(): I32 => 5
primitive PassFlatten fun apply(): I32 => 6
primitive PassTraits fun apply(): I32 => 7
primitive PassDocs fun apply(): I32 => 8
primitive PassRefer fun apply(): I32 => 9
primitive PassExpr fun apply(): I32 => 10
primitive PassCompleteness fun apply(): I32 => 11
primitive PassVerify fun apply(): I32 => 12
primitive PassFinaliser fun apply(): I32 => 13
primitive PassSerialiser fun apply(): I32 => 14
primitive PassReach fun apply(): I32 => 15
primitive PassPaint fun apply(): I32 => 16
primitive PassLLVMIR fun apply(): I32 => 17
primitive PassBitcode fun apply(): I32 => 18
primitive PassASM fun apply(): I32 => 19
primitive PassObj fun apply(): I32 => 20
primitive PassAll fun apply(): I32 => 21

primitive _StrList
  """STUB"""

primitive _MagicPackage
  """STUB"""


primitive _Plugins
  """STUB"""

struct _PassOpt
  var limit: I32 = PassParse()
  var programm_pass: I32 = PassParse()
  var release: Bool = false
  var library: Bool = false
  var runtimebc: Bool = false
  var staticbin: Bool = false
  var pic: Bool = false
  var print_stats: Bool = false
  var verify: Bool = false
  var extfun: Bool = false
  var strip_debug: Bool = false
  var print_filenames: Bool = false
  var check_tree: Bool = false
  var lint_llvm: Bool = false
  var docs: Bool = false
  var docs_private: Bool = false

  var verbosity: I32 = VerbosityQuiet()

  var ast_print_width: USize = 0
  var allow_test_symbols: Bool = false
  var parse_trace: Bool = false

  var package_search_paths: Pointer[_StrList] = package_search_paths.create()
  var safe_packages: Pointer[_StrList] = safe_packages.create()
  var magic_packages: Pointer[_MagicPackage] = magic_packages.create()

  var argv0: Pointer[U8] tag = argv0.create()
  var all_args: Pointer[U8] val = recover val all_args.create() end
  var output: Pointer[U8] tag = output.create()
  var bin_name: Pointer[U8] val = recover val bin_name.create() end

  var link_arch: Pointer[U8] ref = link_arch.create()
  var linker: Pointer[U8] ref = linker.create()
  var link_ldcmd: Pointer[U8] ref = link_ldcmd.create()

  // this field is only present in debug builds
  var llvm_args: Pointer[U8] val = recover val llvm_args.create() end

  var triple: Pointer[U8] ref = triple.create()
  var abi: Pointer[U8] ref = abi.create()
  var cpu: Pointer[U8] ref = cpu.create()
  var features: Pointer[U8] ref = features.create()
  var serialise_id_hash_key: Pointer[U8] ref = serialise_id_hash_key.create()

  embed check: _Typecheck = check.create()
  var plugins: Pointer[_Plugins] ref = plugins.create()
  var userflags: Pointer[_UserFlags] ref = userflags.create()
    """user-provided defines"""
  var data: Pointer[None] ref = data.create() // user-defined data for unit test callbacks

  new ref create() => None
