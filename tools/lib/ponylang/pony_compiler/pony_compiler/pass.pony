use @pass_opt_init[None](options: _PassOpt)
use @pass_opt_done[None](options: _PassOpt)
use @ast_passes_program[Bool](program: Pointer[_AST] val, options: _PassOpt)

type PassId is (
  PassParse      | PassSyntax         | PassSugar        | PassScope  |
  PassImport     | PassNameResolution | PassFlatten      | PassTraits |
  PassRefer      | PassExpr           | PassCompleteness | PassVerify | PassFinaliser |
  PassSerialiser | PassReach          | PassPaint        | PassLLVMIR | PassBitcode |
  PassASM        | PassObj            | PassAll
)

primitive PassParse is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 0
  fun string(): String iso^ => recover String .> append("parse") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassSyntax is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 1
  fun string(): String iso^ => recover String .> append("syntax") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassSugar is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 2
  fun string(): String iso^ => recover String .> append("sugar") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassScope is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 3
  fun string(): String iso^ => recover String .> append("scope") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassImport is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 4
  fun string(): String iso^ => recover String .> append("import") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassNameResolution is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 5
  fun string(): String iso^ => recover String .> append("name resolution") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassFlatten is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 6
  fun string(): String iso^ => recover String .> append("flatten") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassTraits is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 7
  fun string(): String iso^ => recover String .> append("traits") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassRefer is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 8
  fun string(): String iso^ => recover String .> append("refer") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassExpr is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 9
  fun string(): String iso^ => recover String .> append("expr") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassCompleteness is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 10
  fun string(): String iso^ => recover String .> append("completeness") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassVerify is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 11
  fun string(): String iso^ => recover String .> append("verify") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassFinaliser is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 12
  fun string(): String iso^ => recover String .> append("finaliser") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassSerialiser is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 13
  fun string(): String iso^ => recover String .> append("serialiser") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassReach is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 14
  fun string(): String iso^ => recover String .> append("reach") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassPaint is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 15
  fun string(): String iso^ => recover String .> append("paint") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassLLVMIR is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 16
  fun string(): String iso^ => recover String .> append("LLVM IR") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassBitcode is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 17
  fun string(): String iso^ => recover String .> append("bitcode") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassASM is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 18
  fun string(): String iso^ => recover String .> append("asm") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassObj is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 19
  fun string(): String iso^ => recover String .> append("obj") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

primitive PassAll is (Equatable[PassId] & Comparable[PassId])
  fun apply(): I32 => 20
  fun string(): String iso^ => recover String .> append("all") end
  fun eq(that: box->PassId): Bool => this.apply() == that.apply()
  fun lt(that: box->PassId): Bool => this.apply().lt(that.apply())

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
  var sysroot: Pointer[U8] ref = sysroot.create()

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
