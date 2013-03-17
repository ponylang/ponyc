import sys

class AST(object):
  def __init__(self, name, children=None):
    self.type = None
    self.name = name
    if children:
      self.children = children
    else:
      self.children = []

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
