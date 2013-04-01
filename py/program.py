import sys
import urllib.parse
import os.path
from parser import Parser
from package import Package

class Program(object):
  """
  A program contains a parser and a collection of packages.

  FIX: Built-in types should be a package that's imported first.

  parser: lexer, parser and AST generator
  package_map: map of URL to Package
  package_list: list of Package
  """
  builtins = (
    'None', 'Bool', 'String', 'FieldID', 'MethodID',
    'I8', 'I16', 'I32', 'I64', 'I128',
    'U8', 'U16', 'U32', 'U64', 'U128',
    'F32', 'F64',
    'D32', 'D64'
    )

  def __init__(self, optimize, debuglevel):
    debug = True if debuglevel > 0 else False
    optimize = True if optimize else False
    self.parser = Parser(optimize=optimize, debug=debug, debuglevel=debuglevel)
    self.package_map = {}
    self.package_list = []

  def show(self, buf=sys.stdout):
    for package in self.package_list:
      package.show(buf)

  def add_package(self, base, url=None):
    """
    This resolves a package by URL. If the URL has already been loaded, the
    package is simply returned. If a new package is loaded, the SHA-1 hash of
    the package is first checked against other packages to make sure it is not
    a duplicate.

    If it is a duplicate, the new URL is also mapped to the existing package.
    """
    url = self._canonical_url(base, url)
    if url in self.package_map:
      return self.package_map[url]
    else:
      package = Package(self, len(self.package_list), url)
      duplicate = False
      for existing in self.package_list:
        if existing.hash == package.hash:
          package = existing
          duplicate = True
          break
      if not duplicate:
        self.package_list.append(package)
      self.package_map[url] = package
      return package

  def typecheck(self):
    for package in self.package_list:
      package.populate_types()
    for package in self.package_list:
      package.typecheck_params()
    for package in self.package_list:
      package.typecheck_types()
    for package in self.package_list:
      package.typecheck_bodies()

  def parse(self, url, text):
    return self.parser.parse(url, text)

  # Private

  def _canonical_url(self, base, url):
    if url == None:
      url = base
    elif url[0] == '/':
      url = urllib.parse.urljoin(base, url)
    else:
      url = base + '/' + url
    url = urllib.parse.urlparse(url, scheme='file')
    path = url.path
    if url.scheme == 'file':
      path = os.path.expanduser(path)
      path = os.path.expandvars(path)
      path = os.path.realpath(path)
      url = urllib.parse.ParseResult(
        url.scheme, url.netloc, path, url.params, url.query, url.fragment
        )
    url = urllib.parse.urlunparse(url)
    return url
