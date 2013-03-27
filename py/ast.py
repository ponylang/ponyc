import sys
import scopes
import types

class ASTError(Exception):
  pass

class AST(object):
  def __init__(self, name, children=None):
    self.name = name
    self.children = children or []
    self.type = None
    self.typedesc = None
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
      buf.write(lead + '  )\n')

  def typecheck(self, parent=None):
    if parent:
      self.scope = parent.scope
      self.typedesc = parent.typedesc
    else:
      self.scope = scopes.Scope()
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
      raise ASTError('Redefinition of type %s' % name)
    self.typedesc = self.type
    self.scope = scopes.Scope(self.scope)
    return self._resolve_type_params() and self._resolve_implements()

  def _resolve_type_params(self):
    if self.children[1]:
      for param in self.children[1].children:
        param_id = param.children[0]
        if param_id[0].isupper():
          if self.scope.get_type(param_id) != None:
            raise ASTError('Type parameter %s is already bound' % param_id)
            return False
          # FIX: type_opt, default_opt
        else:
          if param_id in self.scope.bound_vars:
            raise ASTError('Value parameter %s is already bound' % param_id)
            return False
          # FIX: type_opt, default_opt
    return True

  def _resolve_implements(self):
    if self.children[2]:
      for t in self.children[2].children:
        # FIX: implements
        # disallow ADT and partial type
        # record pkg.name[arg*]
        pass
    return True

  def _tc_default(self):
    for n in self.children:
      if n:
        n.typecheck(self)

  def _tc_module(self):
    """
    FIX: this should share a package scope with other modules in the package
    """
    self.scope = scopes.PackageScope()
    self._tc_default()

  def _tc_use(self):
    """
    FIX: this should import packages into self.scope
    """
    pass

  def _tc_type_def(self):
    pkg = self.children[0]
    if pkg:
      if pkg in scope.bound_packages:
        raise ASTError('Unimplemented')
      else:
        raise ASTError('No package %s in scope' % pkg)
    # FIX:

  def _tc_actor(self):
    if self._register_type():
      for member in self.children[3].children:
        member.typecheck(self)

  def _tc_class(self):
    """
    FIX: type scope vs type descriptor?


    """
    self.scope = scopes.Scope(self.scope)
    self.typedesc = typecheck.Desc(self.scope, self)
    for member in self.children[3].children:
      if member.name == 'msg':
        raise ASTError('Cannot have msg member in class')
      else:
        member.typecheck(self)

  def _tc_trait(self):
    if self._register_type():
      for member in self.children[3].children:
        if member.name == 'var':
          raise ASTError('Cannot have var member in trait')
        elif member.name == 'val':
          raise ASTError('Cannot have val member in trait')
        else:
          member.typecheck(self)

  def _tc_var(self):
    name = self.children[0]
    if self.children[1] == None:
      raise ASTError('Field %s must specify type' % name)
    for f in self.typedesc.fields:
      if f.name == name:
        raise ASTError('Redefinition of field %s' % name)
    typedesc = self.scope.resolve_type(self.children[1])
    if typedesc == None:
      raise ASTError('Unable to resolve type for field %s' % name)
    f = Field(name, typedesc, false)
    self.typedesc.fields.append(f)

  def _tc_val(self):
    pass

  def _tc_private(self):
    pass

  def _tc_public(self):
    pass
