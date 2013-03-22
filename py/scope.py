import types
import ast

class ScopeError(Exception):
  pass

class PackageScope(object):
  """
  Package scope contains package types and imported packages.

  packages: map of string to PackageScope
  types: map of string to types.Desc
  """
  # FIX: these will have methods on them
  builtins = (
    'None', 'Bool', 'String', 'FieldID', 'MethodID',
    'I8', 'I16', 'I32', 'I64', 'I128',
    'U8', 'U16', 'U32', 'U64', 'U128',
    'F32', 'F64',
    'D32', 'D64'
    )

  def __init__(self):
    self.packages = {}
    self.types = {}
    # FIX: types.Desc for builtins
    for name in PackageScope.builtins:
      self.types[name] = types.Desc(name)

  def import_pkg(self, url):
    """
    FIX: import a package
    """
    pass

  def add_type(self, name, desc):
    """
    Adds a types.Desc to the package
    """
    if name in self.types:
      raise ScopeError('Redefinition of type %s' % name)
    self.types[name] = desc

class TypeScope(object):
  """
  Type scope contains bound formal parameters, fields and methods.

  package: PackageScope
  type: types.Desc
  vars: map of string to types.Expr
  vals: map of string to types.Expr
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
  vars: map of string to types.Expr
  vals: map of string to types.Expr
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
