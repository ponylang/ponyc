import urllib.parse
import os.path

class ModuleError(Exception):
  pass

class Module(object):
  """
  Module contains imported packages and type definitions.

  FIX: Import built-in types into local namespace by default.

  package: Package
  url: string
  packages: map of string to list of Package
  ast: AST
  """
  def __init__(self, package, url):
    self.package = package
    self.url = url
    self.packages = {None: package}
    text = self._load()
    package.update_hash(text)
    self.ast = package.parse(url, text)

  def show(self, buf):
    self.ast.show(buf)

  def import_packages(self):
    self.ast.import_packages(self)

  def add_package(self, name, url, ast):
    """
    Add a package at module scope
    """
    if name in self.packages:
      raise ModuleError(
        """
%s [%s:%s]: redefinition of import %s
        """ %
        (self.url, ast.line, ast.col, name)
        )
    else:
      self.packages[name] = self.package.add_package(url)

  def populate_types(self):
    self.ast.populate_types(self)

  def add_type(self, name, ast):
    self.package.add_type(self, name, ast)

  def resolve_typename(self, pkg, name):
    """
    The pkg and name are strings.
    """
    package = self.packages[pkg]
    if package != None:
      return package.resolve_typedef(name)
    else:
      return None

  # Private

  def _load(self):
    """
    Currently only handles file URLs
    """
    url = urllib.parse.urlparse(self.url)
    if url.scheme == 'file':
      return self._load_file(url)
    else:
      raise ModuleError('Unable to load module: %s' % self.url)

  def _load_file(self, url):
    if url.netloc != '':
      raise ModuleError('Unable to load remote module: %s' % self.url)
    if not os.path.isfile(url.path):
      raise ModuleError('Module is not a file: %s' % self.url)
    with open(url.path, 'rU') as f:
      text = f.read()
    return text
