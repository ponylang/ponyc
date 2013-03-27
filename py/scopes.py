import urllib.request
import urllib.parse
import os
import os.path
import typecheck
import ast
from parser import *

class ScopeError(Exception):
  pass

class ProgramScope(object):
  """
  Scope to hold all packages.

  parser: lexer, parser and AST generator
  packages: map of URL to PackageScope
  """
  def __init__(self, debuglevel=0):
    debug = True if debuglevel > 0 else False
    self.parser = Parser(debug=debug, debuglevel=debuglevel)
    self.packages = {}

  def package(self, url):
    # FIX: canonicalize url?
    if url not in self.packages:
      self.packages[url] = PackageScope(self, url)
    return self.packages[url]

class PackageScope(object):
  """
  Package scope contains modules.

  program: ProgramScope
  url: full url to the package
  modules: list of ModuleScope
  """
  def __init__(self, program, url):
    self.program = program
    self.url = url
    self.modules = []
    self._load()

  # Private

  def _load(self):
    """
    Currently only handles file URLs
    """
    url = urllib.parse.urlparse(self.url)
    if url.scheme != 'file' or url.netloc != '':
      raise ScopeError('Unable to load package: %s' % self.url)
    if not os.path.isdir(url.path):
      raise ScopeError('Package is not a directory: %s' % self.url)
    for filename in os.listdir(url.path):
      if filename.endswith('.pony'):
        if not os.path.isdir(url.path + '/' + filename):
          self.modules.append(ModuleScope(self.program, self.url + '/' + filename))

class ModuleScope(object):
  """
  Module scope contains imported packages and type definitions.

  packages: map of string to list of PackageScope
  types: map of string to typecheck.Desc

  Built-in types should be a package that's always imported.
  """
  builtins = (
    'None', 'Bool', 'String', 'FieldID', 'MethodID',
    'I8', 'I16', 'I32', 'I64', 'I128',
    'U8', 'U16', 'U32', 'U64', 'U128',
    'F32', 'F64',
    'D32', 'D64'
    )

  def __init__(self, program, url):
    self.program = program
    self.url = url
    self.packages = {}
    self.types = {}
    self.ast = self.program.parser.parse(url)
    self.ast.show()

  # Private

  def _use(self, name, url):
    """
    Import a package
    """
    pkg = self.program.package(urllib.parse.urljoin(self.url, url))
    if name in self.packages:
      self.packages[name].append(pkg)
    else:
      self.packages[name] = [pkg]

  def _add_type(self, name, desc):
    """
    Adds a typecheck.Desc to the package
    """
    if name in self.types:
      raise ScopeError('Redefinition of type %s' % name)
    self.types[name] = desc

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
