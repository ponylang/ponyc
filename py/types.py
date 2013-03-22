class TypesError(Exception):
  pass

class Param(object):
  """
  A formal parameter to a type.

  If the name starts with a capital letter, it is a type parameter. Otherwise,
  it is a value parameter. The type is an optional TypeExpr indicating what a
  type parameter must be a subtype of or what a value parameter must be an
  instance of. The default is an optional value for the parameter if it is left
  unspecified.

  name: string
  type: Expr
  default: Expr or AST
  """
  def __init__(self, scope, ast):
    self.name = ast.children[0]
    self.type = scope.resolve_type(ast.children[1])
    if self.istype():
      self.default = scope.resolve_type(ast.children[2])
    else:
      self.default = ast.children[2]
    if self.isvalue() and self.type == None:
      raise TypesError('Formal value parameter must specify a type')
    if self.default != None and
      self.type != None and
      not self.type.instance(self.default):
      raise TypesError('Formal parameter default of invalid type')

  def accept(self, binding):
    """
    Binding should be a TypeExpr for a type parameter and an AST for a value
    parameter.
    """
    if self.istype():
      return self.type.subtype(binding)
    else:
      return self.type.instance(binding)

  def istype(self):
    return self.name[0].isupper()

  def isvalue(self):
    return not self.istype()

class Desc(object):
  """
  Describes a class, trait or actor.

  To have a complete type, a TypeBind is needed. It binds the formal parameter
  types and values in self.parameters to actual types and values.

  self.name: string
  self.parameters: list of TypeParam
  self.implements: list of TypeExpr
  self.fields: list of TypeField
  self.methods: list of TypeMethod
  """
  def __init__(self, scope, ast):
    self.name = ast.children[0]
    self.parameters = []
    self.implements = []
    self.fields = []
    self.methods = []
    scope.package.add_type(self.name, self)
    if ast.children[1]:
      for param in ast.children[1].children:
        self.parameters.append(Param(scope, param))

  def accept(self, parameters):
    """
    Takes a list of parameters, which should each be a TypeExpr for a type
    parameter or an AST for a value parameter.
    """
    if len(self.parameters) != len(parameters):
      return False
    for i in len(self.parameters):
      if not self.parameters[i].accept(parameters[i]):
        return False
    return True

class Field(object):
  def __init__(self, name, binding, val):
    self.name = name
    self.type = binding
    self.val = val

class Method(object):
  def __init__(self):
    pass

class Expr(object):
  """
  Describes the type of an expression. This can be a Bind, Lambda or ADT.
  """
  def __init__(self):
    pass

  def subtype(self, sub):
    return False

class Bind(TypeExpr):
  """
  Describes a bound type.

  This is a TypeDesc with bindings for the formal parameters. It is constructed
  from a TypeDesc and a list of TypeExpr.
  """
  def __init__(self, scope, ast):

    self.typedesc = typedesc
    self.parameters = parameters or []
    if not typedesc.accept(self.parameters):
      raise TypesError('Formal parameters not accepted')

  def subtype(self, sub):
    """
    For sub to be a subtype of self, it must either have the same TypeDesc and
    the same formal bindings as self, or it must implement self with the same
    formal bindings. Formal bindings are invariant.

    An ADT is never a subtype of a TypeBind.
    """
    return False

class Lambda(TypeExpr):
  def __init__(self, scope, ast):
    self.parameters = []
    for child in ast.children[0].children:
      self.parameters.append(scope.resolve_type(child))
    self.result = scope.resolve_type(ast.children[1])

  def subtype(self, sub):
    """
    For sub to be a subtype of self, it must be a TypeLambda where all
    self.parameters are subtypes of all sub.parameters (contravariant) and all
    sub.results are subtypes of all self.results (covariant).

    Also acceptable is a TypeDesc that implements a subtype of this TypeLambda.
    An ADT is never a subtype of a TypeLambda.
    """
    return False

class ADT(TypeExpr):
  def __init__(self, scope, ast):
    self.typelist = []
    for child in ast.children:
      self.typelist.append(scope.resolve_type(child))

  def subtype(self, sub):
    """
    For sub to be a subtype of self, it must be a TypeBind or TypeLambda that is
    a subtype of an element of the ADT, or it must be an ADT where each element
    is a subtype of an element of the ADT.
    """
    return False
