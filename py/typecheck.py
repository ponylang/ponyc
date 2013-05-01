class TypecheckError(Exception):
  pass

class TypeDef(object):
  """
  Defines an actor, class or trait.

  To have a complete type, a ObjectType is needed. It binds the formal parameter
  types and values in self.parameters to actual types and values.

  self.ast: AST
  self.name: string
  self.parameters: list of TypeParam
  self.implements: list of ObjectType|FunctionType
  self.fields: list of Field
  self.methods: list of Method
  """
  state = (
    'initial',
    'checking_params',
    'checked_params'
    )

  def __init__(self, module, ast):
    self.state = 'initial'
    self.module = module
    self.ast = ast
    self.name = ast.children[0]
    self.parameters = []
    self.parameter_map = {}
    self.implements = []
    self.fields = []
    self.methods = []

  def typecheck_params(self):
    if self.state == 'checking_params':
      raise TypecheckError(
        """
%s [%s:%s]: circular type dependency in %s
        """ %
        (self.ast.module.url, self.ast.line, self.ast.col, self.name)
        )
    elif self.state == 'initial':
      self.state = 'checking_params'
      if self.ast.children[1]:
        for ast in self.ast.children[1].children:
          param = TypeParam(self, ast)
          if param.name in self.parameter_map:
            raise TypecheckError(
              """
%s [%s:%s]: redefinition of type parameter %s
              """ %
              (self.module.url, self.ast.line, self.ast.col, param.name)
              )
          else:
            self.parameters.append(param)
            self.parameter_map[param.name] = param
      self.state = 'checked_params'

  def typecheck_type(self):
    """
    FIX: implements, fields, method signatures, check methods allow us to
    satisfy our implements.
    """
    pass

  def typecheck_body(self):
    """
    FIX: method bodies.
    """
    pass

  def accept(self, parameters):
    """
    Takes a list of parameters, which should each be an ADT for a type parameter
    or an AST for a value parameter.
    """
    if len(self.parameters) != len(parameters):
      return False
    for i in range(len(self.parameters)):
      if not self.parameters[i].accept(parameters[i]):
        return False
    return True

  def resolve_adt(self, ast):
    """
    The ast is an AST('type') or None. Return an ADT containing each type in
    ast.children.
    """
    typelist = []
    if ast != None:
      for child in ast.children:
        typelist.append(self._resolve_type(child))
    return ADT(typelist)

  def resolve_typedef(self, pkg, name):
    """
    The pkg and name are strings. We first check for them in our type params.
    Then we check for them in our module. If we reference a type that hasn't
    checked its parameters yet, check them.
    """
    if pkg == None and name in self.parameter_map:
      return self.parameter_map[name].type
    else:
      return self.module.resolve_typedef(pkg, name)

  # Private

  def _resolve_type(self, ast):
    """
    The ast is a base_type, a partial_type, or a lambda.
    """
    if ast.name == 'lambda':
      return FunctionType(self, ast)
    else:
      return ObjectType(self, ast)

class TypeParam(object):
  """
  A formal parameter to a type.

  If the name starts with a capital letter, it is a type parameter. Otherwise,
  it is a value parameter. The type is an optional Expr indicating what a
  type parameter must be a subtype of or what a value parameter must be an
  instance of. The default is an optional value for the parameter if it is left
  unspecified.

  name: string
  type: ADT
  FIX: default value using ast.children[2]
  """
  def __init__(self, typedef, ast):
    self.name = ast.children[0]
    self.type = typedef.resolve_adt(ast.children[1])

  def accept(self, binding):
    """
    Binding is an ADT for a type parameter and an AST for a value parameter.
    """
    if self.istype():
      return self.type.subtype(binding)
    else:
      return self.type.instance(binding)

  def istype(self):
    return self.name[0].isupper()

  def isvalue(self):
    return not self.istype()

class Field(object):
  def __init__(self, name, binding, val):
    self.name = name
    self.type = binding
    self.val = val

class Method(object):
  def __init__(self):
    pass

class ADT(object):
  """
  An ADT with no elements is the top type.

  typelist: list of ObjectType|FunctionType
  """
  def __init__(self, typelist):
    self.typelist = []
    for t in typelist:
      self._add_type(t)

  def subtype(self, sub):
    """
    If this ADT is the top type, everything is a subtype.
    """
    if len(self.typelist) == 0:
      return True
    elif isinstance(sub, ADT):
      return self._sub_adt(sub)
    else:
      return self._sub_type(sub)

  # Private

  def _add_type(self, t):
    if isinstance(t, ADT):
      for elem in t.typelist:
        self._add_type(elem)
    else:
      self.typelist.append(t)

  def _sub_adt(self, sub):
    """
    An ADT is a subtype of this ADT if every element in that ADT is a subtype of
    an element in this ADT, and that ADT is not the top type.
    """
    for elem in sub.typelist:
      if not self._sub_type(elem):
        return False
    return len(sub.typelist) != 0

  def _sub_type(self, sub):
    """
    A type is a subtype of an ADT if it is a subtype of an element of the ADT.
    """
    for elem in self.typelist:
      if elem.subtype(sub):
        return True
    return False

class ObjectType(object):
  """
  partial: Boolean
  type: TypeDef
  parameters: list of ADT
  """
  def __init__(self, typedef, ast):
    """
    Resolve using typedef as the scope.
    """
    self.partial = ast.name == 'partial_type'
    self.type = typedef.resolve_typedef(ast.children[0], ast.children[1])
    self.parameters = []
    if self.type == None:
      raise TypecheckError(
        """
%s [%s:%s]: cannot resolve type %s.%s
        """ %
        (typedef.module.url, ast.line, ast.col,
          ast.children[0], ast.children[1])
        )
    self._extract_params(ast)

  def subtype(self, sub):
    if isinstance(sub, ObjectType):
      return self._sub_object(sub)
    elif isinstance(sub, FunctionType):
      return self._sub_function(sub)
    elif isinstance(sub, ADT):
      return self._sub_adt(sub)
    else:
      return False

  # Private

  def _extract_params(self, ast):
    if ast.children[2] != None:
      for param in ast.children[2].children:
        self._extract_param(param)

  def _extract_param(self, ast):
    """
    NEXTUP:
    FIX: the ast is an expr. Need to determine if it is a type or a value.
    """
    pass

  def _sub_object(self, sub):
    """
    FIX: True if sub implements our type with the same formal parameters.
    What about if we are a partial type?
    """
    if self.typedef == None:
      return True
    else:
      return False

  def _sub_function(self, sub):
    """
    A function is only a subtype of an object if the object has no typedef.
    FIX: what about Any?
    """
    if self.typedef == None:
      return True
    else:
      return False

  def _sub_adt(self, sub):
    """
    True if all elements of the ADT are a subtype of this ObjectType and the ADT
    is not the top type.
    """
    for elem in sub.typelist:
      if not self.subtype(elem):
        return False
    return len(sub.typelist) != 0

class FunctionType(object):
  """
  parameters: list of ADT
  result: ADT
  """
  def __init__(self, typedef, ast):
    self.parameters = []
    if ast.children[0]:
      for child in ast.children[0].children:
        self.parameters.append(typedef.resolve_adt(child))
    self.result = typedef.resolve_adt(ast.children[1])

  def subtype(self, sub):
    if isinstance(sub, ObjectType):
      return self._sub_object(sub)
    elif isinstance(sub, FunctionType):
      return self._sub_function(sub)
    elif isinstance(sub, ADT):
      return self._sub_adt(sub)
    else:
      return False

  # Private

  def _sub_object(self, sub):
    """
    FIX: True if sub implements this FunctionType
    """
    return False

  def _sub_function(self, sub):
    """
    True if all parameters are contravariant and all results are covariant.
    """
    if len(self.parameters) != len(sub.parameters):
      return False
    for i in range(len(self.parameters)):
      if not sub.parameters[i].subtype(self.parameters[i]):
        return False
    if not self.result.subtype(sub.result):
      return False
    return True

  def _sub_adt(self, sub):
    """
    True if all elements of the ADT are a subtype of this FunctionType and the
    ADT is not the top type.
    """
    for elem in sub.typelist:
      if not self.subtype(elem):
        return False
    return len(sub.typelist) != 0
