import sys

class TypeError(object):
  pass

class Type(object):
  def __init__(self, name):
    self.name = name
    self.parameters = []
    self.implements = []
    self.fields = []
    self.methods = []

class Scope(object):
  # FIX: these will have methods on them
  builtins = (
    'None', 'Bool', 'String', 'FieldID', 'MethodID',
    'I8', 'I16', 'I32', 'I64', 'I128',
    'U8', 'U16', 'U32', 'U64', 'U128',
    'F32', 'F64',
    'D32', 'D64'
    )

  def __init__(self, parent=None):
    self.parent = parent
    self.bound_packages = {}
    self.bound_types = {}
    self.bound_vars = {}
    self.vars = {}
    self.vals = {}
    self.private_methods = {}
    self.public_methods = {}
    if parent == None:
      for name in Scope.builtins:
        self.bound_types[name] = Type(name)

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

  def add_type(self, name):
    if name in self.bound_types:
      return None
    typedesc = Type(name)
    self.bound_types[name] = typedesc
    return typedesc

class AST(object):
  def __init__(self, name, children=None):
    self.name = name
    self.children = children or []
    self.type = None
    self.containing_type = None
    self.scope = None

  def __repr__(self):
    return 'AST("%s","%s")' % (self.name, self.children)

  def show(self, buf=sys.stdout, indent=0):
    lead = ' ' * indent
    if (self._showlen() + indent) < 80:
      buf.write(lead + '(' + self.name)
      for child in self.children:
        if child == None:
          buf.write(' ()')
        elif isinstance(child, AST):
          child.show(buf, 1)
        else:
          buf.write(' ' + str(child))
      buf.write(')')
    else:
      buf.write(lead + '(' + self.name + '\n')
      for child in self.children:
        if child == None:
          buf.write(lead + '  ()\n')
        elif isinstance(child, AST):
          child.show(buf, indent + 2)
          buf.write('\n')
        else:
          buf.write(lead + '  ' + str(child) + '\n')
      buf.write(lead + '  )')

  def typecheck(self, parent=None):
    if parent:
      self.scope = parent.scope
      self.containing_type = parent.containing_type
    else:
      self.scope = Scope()
    getattr(self, '_tc_' + self.name, self._tc_default)()

  # Private

  def _showlen(self):
    l = 2 + len(self.name) + len(self.children)
    for child in self.children:
      if child == None:
        l += 2
      elif isinstance(child, AST):
        l += child._showlen()
      else:
        l += len(str(child))
    return l

  def _register_type(self):
    name = self.children[0]
    self.type = self.scope.add_type(name)
    if self.type == None:
      self._tc_error('Redefinition of type %s' % name)
      return False
    self.containing_type = self.type
    self.scope = Scope(self.scope)
    return self._resolve_type_params() and self._resolve_implements()

  def _resolve_type_params(self):
    if self.children[1]:
      for param in self.children[1].children:
        param_id = param.children[0]
        if param_id[0].isupper():
          if self.scope.get_type(param_id) != None:
            self._tc_error('Type parameter %s is already bound' % param_id)
            return False
          # FIX: type_opt, default_opt
        else:
          if param_id in self.scope.bound_vars:
            self._tc_error('Value parameter %s is already bound' % param_id)
            return False
          # FIX: type_opt, default_opt

  def _resolve_implements(self):
    if self.children[2]:
      for t in self.children[2].children:
        # FIX: implements
        # disallow ADT and partial type
        # record pkg.name[arg*]
        pass
    return True

  def _tc_error(self, msg):
    raise TypeError(msg)

  def _tc_default(self):
    for n in self.children:
      if n:
        n.typecheck(self)

  def _tc_use(self):
    # FIX:
    pass

  def _tc_type_def(self):
    pkg = self.children[0]
    if pkg:
      if pkg in scope.bound_packages:
        self._tc_error('Unimplemented')
        return
      else:
        self._tc_error('No package %s in scope' % pkg)
        return
    # FIX:

  def _tc_actor(self):
    if self._register_type():
      for member in self.children[3].children:
        member.typecheck(self)

  def _tc_class(self):
    if self._register_type():
      for member in self.children[3].children:
        if member.name == 'msg':
          self._tc_error('Cannot have msg member in class')
        else:
          member.typecheck(self)

  def _tc_trait(self):
    if self._register_type():
      for member in self.children[3].children:
        if member.name == 'var':
          self._tc_error('Cannot have var member in trait')
        elif member.name == 'val':
          self._tc_error('Cannot have val member in trait')
        else:
          member.typecheck(self)

  def _tc_var(self):
    pass

  def _tc_val(self):
    pass

  def _tc_private(self):
    pass

  def _tc_public(self):
    pass
