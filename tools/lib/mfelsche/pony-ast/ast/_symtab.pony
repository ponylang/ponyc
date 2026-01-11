use @symtab_new[Pointer[_Symtab] ref]()
use @symtab_free[None](symtab: Pointer[_Symtab] box) // box is needed as this is
// called in a finalizer
use @symtab_add[Bool](symtab: Pointer[_Symtab] ref, name: Pointer[U8] tag, def: Pointer[_AST] box, status: NullablePointer[SymStatus])
use @symtab_find[Pointer[_AST] val](symtab: Pointer[_Symtab] box, name: Pointer[U8] tag, status: NullablePointer[SymStatus])
use @symtab_init[None](symtab: Pointer[_Symtab] ref, size: USize)
use @symtab_destroy[None](symtab: Pointer[_Symtab] ref)
use @symtab_optimize[None](symtab: Pointer[_Symtab] ref)
use @symtab_get[NullablePointer[_Symbol]](symtab: Pointer[_Symtab] box, key: NullablePointer[_Symbol], index: Pointer[USize])
use @symtab_put[NullablePointer[_Symbol]](symtab: Pointer[_Symtab] ref, entry: NullablePointer[_Symbol])
use @symtab_putindex[None](symtab: Pointer[_Symbol] ref, entry: NullablePointer[_Symbol], index: USize)
use @symtab_size[USize](symtab: Pointer[_Symtab] box)
use @symtab_next[NullablePointer[_Symbol]](symtab: Pointer[_Symtab] box, i: Pointer[USize])

use "debug"


struct _Symbol
  """
  typedef struct symbol_t
  {
    const char* name;
    ast_t* def;
    sym_status_t status;
    size_t branch_count;
  } symbol_t;
  """
  let name: Pointer[U8] val = name.create()
  let def: Pointer[_AST] val = def.create()
  let status: SymStatus = status.create()
  let branch_count: USize = 0

struct _Symtab
  """
  stupid stub
  """

struct SymStatus
  var status: I32 = SymStati.none()

  new ref create() => None

  fun box string(): String iso^ =>
    recover iso 
      String.create() .> append(match status
      | SymStati.none() => "NONE"
      | SymStati.nocase() => "NO_CASE"
      | SymStati.defined() => "DEFINED"
      | SymStati.undefined() => "UNDEFINED"
      | SymStati.consumed() => "CONSUMED"
      | SymStati.consumed_same_expr() => "CONSUMED_SAME_EXPR"
      | SymStati.ffidecl() => "FFIDECL"
      | SymStati.err() => "ERROR"
      else
        "IMPOSSIBLE"
      end)
    end

primitive SymStati
  fun none(): I32 => 0
  fun nocase(): I32 => 1
  fun defined(): I32 => 3
  fun undefined(): I32 => 4
  fun consumed(): I32 => 5
  fun consumed_same_expr(): I32 => 6
  fun ffidecl(): I32 => 7
  fun err(): I32 => 8


class ref SymbolTable
  let ptr: Pointer[_Symtab]
  let _owned_alloc: Bool

  new from_pointer(ptr': Pointer[_Symtab]) =>
    ptr = ptr'
    _owned_alloc = false

  new iso create(size': USize = 0) =>
    ptr = @symtab_new()
    @symtab_init(ptr, size')
    _owned_alloc = true

  fun box apply(name: String box): (AST | None) =>
    var status = SymStatus.create()
    let ptr' = @symtab_find(ptr, name.cpointer(), NullablePointer[SymStatus](status))
    if ptr'.is_null() then
      None
    else
      AST(ptr')
    end
    
  fun box size(): USize =>
    @symtab_size(ptr)

  fun box _final() =>
    if _owned_alloc then
      @symtab_free(ptr)
    end

  fun iter(): Iterator[(String, AST)] =>
    _SymbolTableIter[this->SymbolTable](this)

  fun debug(): String val =>
    var s = recover val String() end
    for (name, definition) in this.iter() do
      s = s + name + ": " + definition.debug() + ", "
    end
    s

// TODO: reason about Iterator element types
//       They should actually be `(S->String, S->AST)`
//       but the capabilities don't allow for that unless we redesign all of the
//       AST creation
class _SymbolTableIter[S: SymbolTable #read] is Iterator[(String, AST)]
  let _table: S
  var _i: USize
  var _count: USize

  new create(table: S) =>
    _table = table
    _i = -1 // HASHMAP_BEGIN
    _count = 0

  fun has_next(): Bool =>
    _count < _table.size()

  fun ref next(): (String, AST) ? =>
    let ptr = @symtab_next(_table.ptr, addressof _i)
    let symbol = ptr()?
    _count = _count + 1
    let symbol_name =
      recover val
        String.copy_cstring(symbol.name)
      end
    (symbol_name, AST(symbol.def))


