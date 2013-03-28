import urllib.parse
import os
import os.path
import hashlib
from module import Module

class PackageError(Exception):
  pass

class Package(object):
  """
  A package contains modules.

  program: containing Program
  url: full url to the package
  modules: list of Module
  types: map of string to AST
  hash: sha-1 hash of entire package
  """
  def __init__(self, program, idx, url):
    self.program = program
    self.idx = idx
    self.url = url
    self.modules = []
    self.types = {}
    self.hash = hashlib.sha1()
    self._load()

  def update_hash(self, text):
    self.hash.update(text.encode('utf-8'))

  def parse(self, url, text):
    return self.program.parse(url, text)

  def add_package(self, url):
    return self.program.add_package(self.url, url)

  def add_type(self, name, ast):
    if name in self.types:
      prev = self.types[name]
      raise PackageError(
        """
  %s [%s:%s]: redefinition of type %s
  %s [%s:%s]: original definition of type %s
        """ %
        (ast.module.url, ast.line, ast.col, name,
          prev.module.url, prev.line, prev.col, name)
        )
    self.types[name] = ast

  def populate_types(self):
    for module in self.modules:
      module.populate_types()

  def typecheck_types(self):
    """
    FIX:
    """
    pass

  def typecheck_bodies(self):
    """
    FIX:
    """
    pass

  # Private

  def _load(self):
    """
    Currently only handles file URLs
    """
    url = urllib.parse.urlparse(self.url)
    if url.scheme == 'file':
      self._load_file(url)
    else:
      raise PackageError('Unable to load package: %s' % self.url)
    for module in self.modules:
      module.import_packages()

  def _load_file(self, url):
    if url.netloc != '':
      raise PackageError('Unable to load remote package: %s' % self.url)
    if not os.path.isdir(url.path):
      raise PackageError('Package is not a directory: %s' % self.url)
    for filename in sorted(os.listdir(url.path)):
      if filename.endswith('.pony'):
        if not os.path.isdir(os.path.join(url.path, filename)):
          self.modules.append(Module(self, self.url + '/' + filename))
