class TypecheckError(Exception):
  pass

class TypeDef(object):
  """
  Defines an actor, class or trait.

  To have a complete type, a ClassType is needed. It binds the formal parameter
  types and values in self.parameters to actual types and values.

  self.ast: AST
  self.name: string
  self.parameters: list of TypeParam
  self.implements: list of ClassType|LambdaType
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
    self.state = 'checking_params'
    if self.ast.children[1]:
      for ast in self.ast.children[1].children:
        self.parameters.append(TypeParam(self, ast))
    self.state = 'checked_params'

  def typecheck_type(self):
    """
    FIX: implements, fields, method signatures
    """
    pass

  def typecheck_body(self):
    """
    FIX: method bodies
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
    If ast is None, return None. Otherwise, ast must be an AST('type'). Return
    an ADT containing each type in ast.children.
    """
    if ast == None:
      return None
    else:
      typelist = []
      for child in ast.children:
        typelist.append(self._resolve_type(child))
      return ADT(typelist)

  # Private

  def _resolve_type(self, ast):
    """
    The ast will be a base_type, a partial_type, or a lambda.
    """
    if ast.name == 'lambda':
      return LambdaType(self, ast)
    else:
      return ClassType(self, ast)

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
    if self.isvalue() and self.type == None:
      raise TypecheckError('Formal value parameter must specify a type')

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
  typelist: list of ClassType|LambdaType
  """
  def __init__(self, typelist):
    self.typelist = []
    for t in typelist:
      self._add_type(self, t)

  def subtype(self, sub):
    if isinstance(sub, ADT):
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
    an element in this ADT.
    """
    for elem in sub.typelist:
      if not self._sub_type(elem):
        return False
    return True

  def _sub_type(self, sub):
    """
    A type is a subtype of an ADT if it is a subtype of an element of the ADT.
    """
    for elem in self.typelist:
      if elem.subtype(sub):
        return True
    return False

class ClassType(object):
  """
  partial: Boolean
  type: TypeDef
  parameters: list of ADT
  """
  def __init__(self, typedef, ast):
    """
    FIX: resolve using typedef as the scope

    Need to be fully formed, but can use type parameters already defined in
    typedef. If we reference a type that hasn't checked its parameters yet, check
    them. Need to keep track of recursion - if we come back to a type that is
    in-progress, throw an exception.

    What's stored?

    Real type: typedef, type parameter bindings
      bindings are resolved from the typedef we appear in, not our own
    Type parameter: type param, which has no type parameters
    """
    self.partial = ast.name == 'partial_type'
    self.type = None
    self.parameters = []

  def subtype(self, sub):
    if isinstance(sub, ClassType):
      return self._sub_class(sub)
    elif isinstance(sub, ADT):
      return self._sub_adt(sub)
    else:
      return False

  # Private

  def _sub_class(self, sub):
    """
    FIX: True if sub implements our type with the same formal parameters.
    What about if we are a partial type?
    """
    return False

  def _sub_adt(self, sub):
    """
    True if all elements of the ADT are a subtype of this ClassType.
    """
    for elem in sub.typelist:
      if not self.subtype(elem):
        return False
    return True

class LambdaType(object):
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
    if isinstance(sub, ClassType):
      return self._sub_class(sub)
    elif isinstance(sub, LambdaType):
      return self._sub_lambda(sub)
    elif isinstance(sub, ADT):
      return self._sub_adt(sub)
    else:
      return False

  # Private

  def _sub_class(self, sub):
    """
    FIX: True if sub implements this LambdaType
    """
    return False

  def _sub_lambda(self, sub):
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
    True if all elements of the ADT are a subtype of this Lambda.
    """
    for elem in sub.typelist:
      if not self.subtype(elem):
        return False
    return True
