#!/usr/bin/env python3

import argparse
import os.path
import urllib.parse
from scopes import *

args = argparse.ArgumentParser()
args.add_argument('-p', '--project', help='Pony project URL', default='.')
args.add_argument('-d', '--debug', help='Debug level', type=int, default=0)
args.add_argument('-o', '--optimize', help='Optimize the lexer and parser',
  type=int, default=0)
args.add_argument('-s', '--show', help='Show AST', action="store_true")
arg = args.parse_args()

url = urllib.parse.urlparse(arg.project, scheme='file')
path = url.path
if url.scheme == 'file':
  path = os.path.expanduser(path)
  path = os.path.expandvars(path)
  path = os.path.realpath(path)
  if not os.path.isdir(path):
    path = os.path.dirname(path) + '/'
  url = urllib.parse.ParseResult(
    url.scheme, url.netloc, path, url.params, url.query, url.fragment
    )
url = urllib.parse.urlunparse(url)

program = ProgramScope(arg.debug)
program.package(url)

"""
FIX:
parse when loading packages
insert type definitions
  check for conflicts
typecheck type parameters
typecheck type bodies
generate code
"""
