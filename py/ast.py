class AST(object):
  def __init__(self, p, name, children=None):
    self.name = name
    self.children = children or []
    self.line = 0
    self.col = 0
    for i in range(len(p)):
      if p.lineno(i):
        self.line = p.lineno(i)
        self.col = p.lexer.column(p.lexpos(i))
        break

  def __repr__(self):
    return 'AST("%s",%s)' % (self.name, self.children)

  def show(self, buf, indent=0):
    if self._show_len(indent) < 80:
      self._show_compact(buf, indent)
    else:
      self._show_extended(buf, indent)
    buf.write('\n')

  def import_packages(self, module):
    getattr(self, '_import_' + self.name, self._pass)(module)

  def populate_types(self, module):
    getattr(self, '_populate_' + self.name, self._pass)(module)

  # Private

  indentation = '  '

  def _show_len(self, indent=0):
    l = 2 + len(self.indentation * indent) + len(self.name) + len(self.children)
    for child in self.children:
      if child == None:
        l += 2
      elif isinstance(child, AST):
        l += child._show_len()
      else:
        l += len(str(child))
    return l

  def _show_compact(self, buf, indent=0):
    buf.write(self.indentation * indent)
    buf.write('(' + self.name)
    for child in self.children:
      buf.write(' ')
      if child == None:
        buf.write('()')
      elif isinstance(child, AST):
        child._show_compact(buf)
      else:
        buf.write(str(child))
    buf.write(')')

  def _show_extended(self, buf, indent):
    lead = self.indentation * indent
    lead1 = lead + self.indentation
    buf.write(lead + '(' + self.name + '\n')
    for child in self.children:
      if child == None:
        buf.write(lead1 + '()\n')
      elif isinstance(child, AST):
        child.show(buf, indent + 1)
      else:
        buf.write(lead1 + str(child) + '\n')
    buf.write(lead1 + ')')

  def _pass(self, arg=None):
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
