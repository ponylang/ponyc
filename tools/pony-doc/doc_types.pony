class val DocProgram
  """Top-level IR node representing an entire compiled Pony program."""
  let name: String
  let packages: Array[DocPackage] val

  new val create(name': String, packages': Array[DocPackage] val) =>
    name = name'
    packages = packages'

class val DocPackage
  """A Pony package with its documented types and source files."""
  let qualified_name: String
  let doc_string: (String | None)
  let public_types: Array[DocEntity] val
  let private_types: Array[DocEntity] val
  let source_files: Array[DocSourceFile] val

  new val create(
    qualified_name': String,
    doc_string': (String | None),
    public_types': Array[DocEntity] val,
    private_types': Array[DocEntity] val,
    source_files': Array[DocSourceFile] val)
  =>
    qualified_name = qualified_name'
    doc_string = doc_string'
    public_types = public_types'
    private_types = private_types'
    source_files = source_files'

class val DocEntity
  """
  A documented Pony type (class, actor, trait, interface, primitive,
  struct, or type alias) with its members.

  Note: there is no `private_fields` array. docgen.c only collects
  public fields (`allow_public=true, allow_private=false` at line 1050),
  regardless of the `--include-private` setting.
  """
  let kind: EntityKind
  let name: String
  let tqfn: String
  let type_params: Array[DocTypeParam] val
  let cap: (String | None)
  let provides: Array[DocType] val
  let doc_string: (String | None)
  let constructors: Array[DocMethod] val
  let public_fields: Array[DocField] val
  let public_behaviours: Array[DocMethod] val
  let public_functions: Array[DocMethod] val
  let private_behaviours: Array[DocMethod] val
  let private_functions: Array[DocMethod] val
  let source: (DocSourceLocation | None)

  new val create(
    kind': EntityKind,
    name': String,
    tqfn': String,
    type_params': Array[DocTypeParam] val,
    cap': (String | None),
    provides': Array[DocType] val,
    doc_string': (String | None),
    constructors': Array[DocMethod] val,
    public_fields': Array[DocField] val,
    public_behaviours': Array[DocMethod] val,
    public_functions': Array[DocMethod] val,
    private_behaviours': Array[DocMethod] val,
    private_functions': Array[DocMethod] val,
    source': (DocSourceLocation | None))
  =>
    kind = kind'
    name = name'
    tqfn = tqfn'
    type_params = type_params'
    cap = cap'
    provides = provides'
    doc_string = doc_string'
    constructors = constructors'
    public_fields = public_fields'
    public_behaviours = public_behaviours'
    public_functions = public_functions'
    private_behaviours = private_behaviours'
    private_functions = private_functions'
    source = source'

class val DocMethod
  """A documented method (constructor, function, or behaviour)."""
  let kind: MethodKind
  let name: String
  let cap: (String | None)
  let type_params: Array[DocTypeParam] val
  let params: Array[DocParam] val
  let return_type: (DocType | None)
  let is_partial: Bool
  let doc_string: (String | None)
  let source: (DocSourceLocation | None)

  new val create(
    kind': MethodKind,
    name': String,
    cap': (String | None),
    type_params': Array[DocTypeParam] val,
    params': Array[DocParam] val,
    return_type': (DocType | None),
    is_partial': Bool,
    doc_string': (String | None),
    source': (DocSourceLocation | None))
  =>
    kind = kind'
    name = name'
    cap = cap'
    type_params = type_params'
    params = params'
    return_type = return_type'
    is_partial = is_partial'
    doc_string = doc_string'
    source = source'

class val DocField
  """A documented field (var, let, or embed). Only public fields are collected."""
  let kind: FieldKind
  let name: String
  let field_type: DocType
  let doc_string: (String | None)
  let source: (DocSourceLocation | None)

  new val create(
    kind': FieldKind,
    name': String,
    field_type': DocType,
    doc_string': (String | None),
    source': (DocSourceLocation | None))
  =>
    kind = kind'
    name = name'
    field_type = field_type'
    doc_string = doc_string'
    source = source'

class val DocParam
  """A method parameter with its type and optional default value."""
  let name: String
  let param_type: DocType
  let default_value: (String | None)

  new val create(
    name': String,
    param_type': DocType,
    default_value': (String | None))
  =>
    name = name'
    param_type = param_type'
    default_value = default_value'

class val DocTypeParam
  """A type parameter with optional constraint and default type."""
  let name: String
  let constraint: (DocType | None)
  let default_type: (DocType | None)

  new val create(
    name': String,
    constraint': (DocType | None),
    default_type': (DocType | None))
  =>
    name = name'
    constraint = constraint'
    default_type = default_type'
