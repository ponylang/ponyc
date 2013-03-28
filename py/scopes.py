class ScopeError(Exception):
  pass

class TypeScope(object):
  """
  Type scope contains bound formal parameters, fields and methods.

  package: PackageScope
  type: typecheck.Desc
  vars: map of string to typecheck.Expr
  vals: map of string to typecheck.Expr
  private_methods: map of string to ?
  public_methods: map of string to ?
  """
  def __init__(self, package, desc):
    self.package = package
    self.type = desc
    self.vars = {}
    self.vals = {}
    self.private_methods = {}
    self.public_methods = {}

class Scope(object):
  """
  A lexical scope.

  type: TypeScope
  parent: Scope
  vars: map of string to typecheck.Expr
  vals: map of string to typecheck.Expr
  """
  def __init__(self, typescope, parent=None):
    self.type = typescope
    self.parent = parent
    self.vars = {}
    self.vals = {}

  def get_type(self, name, pkg=None):
    if pkg:
      if pkg in self.bound_packages:
        return self.bound_packages[pkg].get_type(name)
      elif self.parent:
        return self.parent.get_type(name, pkg)
    else:
      if name in self.bound_types:
        return self.bound_types[name]
      elif self.parent:
        return self.parent.get_type(name)
    return None

  def resolve_type(self, ast):
    if ast.name != 'type':
      return None
    for t in ast.children:
      if t.name == 'lambda':
        # FIX:
        return None
