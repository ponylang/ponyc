import sys

class AST(object):
  def __init__(self, p, name, children=None):
    self.name = name
    self.children = children or []
    self.module = None
    self.line = 0
    self.col = 0
    for i in range(len(p)):
      self.line = p.lineno(i)
      self.col = p.lexer.column(p.lexpos(i))
      if self.line != 0:
        break

  def __repr__(self):
    return 'AST("%s",%s)' % (self.name, self.children)

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

  def import_packages(self, module):
    getattr(self, '_import_' + self.name, self._pass)(module)

  def populate_types(self, module):
    getattr(self, '_populate_' + self.name, self._pass)(module)

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

  def _pass(self, module):
    pass

  def _import_module(self, module):
    self.children[0].import_packages(module)

  def _import_use_list(self, module):
    for use in self.children:
      if use != None:
        use.import_packages(module)

  def _import_use(self, module):
    module.add_package(self.children[0], self.children[1])

  def _populate_module(self, module):
    self.children[1].populate_types(module)

  def _populate_type_def_list(self, module):
    for child in self.children:
      if child != None:
        child.module = module
        child.populate_types(module)

  def _populate_type_def(self, module):
    """
    FIX: type
    """
    pass

  def _populate_actor(self, module):
    module.add_type(self.children[0], self)

  def _populate_class(self, module):
    module.add_type(self.children[0], self)

  def _populate_trait(self, module):
    module.add_type(self.children[0], self)
