import ADT from typecheck

class ScopeError(Exception):
  pass

class Scope(object):
  """
  A lexical scope.

  module: Module
  type: TypeDef
  parent: Scope
  vars: map of string to ADT
  vals: map of string to ADT
  """
  def __init__(self, module, typedef=None, parent=None):
    self.module = module
    self.type = typedef
    self.parent = parent
    self.vars = {}
    self.vals = {}

  def make_typescope(self, typedef):
    return Scope(self.module, typedef, self)

  def make_child(self):
    return Scope(self.module, self.typedef, self)

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

  def resolve_typename(self, pkg, name):
    """
    The pkg and name are strings. We first check for them in our type params.
    Then we check for them in our module. If we reference a type that hasn't
    checked its parameters yet, check them.
    """
    if pkg == None and name in self.type.parameter_map:
      return self.type.parameter_map[name].type
    else:
      return self.module.resolve_typename(pkg, name)

  # Private

  def _resolve_type(self, ast):
    """
    The ast is a base_type, a partial_type, or a lambda.
    """
    if ast.name == 'lambda':
      return FunctionType(self, ast)
    else:
      return ObjectType(self, ast)
