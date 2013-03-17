from ast import AST

class TypeError(Exception): pass

class PonyTyper(object):
  """
  A type checker for the Pony language.
  """
  builtins = (
    'None', 'Bool', 'String', 'FieldID', 'MethodID',
    'I8', 'I16', 'I32', 'I64', 'I128',
    'U8', 'U16', 'U32', 'U64', 'U128',
    'F32', 'F64',
    'D32', 'D64'
    )

  def __init__(self):
    self.import_map = {}
    self.type_map = {}
    for builtin in self.builtins:
      self.type_map[builtin] = builtin

  def typecheck(self, ast):
    self._use(ast.children[0])
    self._types(ast.children[1])

  # Private

  def _use(self, ast):
    for use in ast.children:
      if use != None:
        print( 'Import %s as %s' % (use.children[1], use.children[1]))

  def _types(self, ast):
    for t in ast.children:
      if t != None:
        if t.name == 'type_def':
          self._type_def(t)
        elif t.name == 'class':
          self._class(t)
        elif t.name == 'trait':
          self._trait(t)
        elif t.name == 'actor':
          self._actor(t)

  def _type_def(self, ast):
    pass

  def _class(self, ast):
    pass

  def _trait(self, ast):
    pass

  def _actor(self, ast):
    pass
