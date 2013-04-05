class ScopeError(Exception):
  pass

class Scope(object):
  """
  A lexical scope.

  type: TypeDef
  parent: Scope
  vars: map of string to ADT
  vals: map of string to ADT
  """
  def __init__(self, typedef, parent=None):
    self.type = typedef
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
