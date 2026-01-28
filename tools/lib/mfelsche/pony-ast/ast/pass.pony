use @pass_opt_init[None](options: _PassOpt)
use @pass_opt_done[None](options: _PassOpt)
use @ast_passes_program[Bool](program: Pointer[_AST], options: _PassOpt)

type PassId is I32

primitive _StrList
  """STUB"""

primitive _MagicPackage
  """STUB"""


primitive _Plugins
  """STUB"""


primitive PassIds
  fun parse(): I32 => 0
  fun syntax(): I32 => 1
  fun sugar(): I32 => 2
  fun scope(): I32 => 3
  fun import(): I32 => 4
  fun name_resolution(): I32 => 5
  fun flatten(): I32 => 6
  fun traits(): I32 => 7
  fun docs(): I32 => 8
  fun refer(): I32 => 9
  fun expr(): I32 => 10
  fun completeness(): I32 => 11
  fun verify(): I32 => 12
  fun finaliser(): I32 => 13
  fun serialiser(): I32 => 14
  fun reach(): I32 => 15
  fun paint(): I32 => 16
  fun llvm_ir(): I32 => 17
  fun bitcode(): I32 => 18
  fun asm(): I32 => 19
  fun obj(): I32 => 20
  fun all(): I32 => 21

struct _PassOpt
  var limit: PassId = PassIds.parse()
  var programm_pass: PassId = PassIds.parse()
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

  var verbosity: VerbosityLevel = VerbosityLevels.quiet()

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
  var data: Pointer[None] ref = data.create() // user-defined data for unit test callbacks

  new ref create() => None

