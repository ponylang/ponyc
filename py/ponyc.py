#!/usr/bin/env python3

import argparse
from pony_parser import PonyParser
from pony_typer import PonyTyper

args = argparse.ArgumentParser()
args.add_argument('-f', '--filename', help='Pony file')
args.add_argument('-d', '--debug', help='Debug level', type=int, default=0)
args.add_argument('-o', '--optimize', help='Optimize the lexer and parser',
  type=int, default=0)
args.add_argument('-s', '--show', help='Show AST', action="store_true")
arg = args.parse_args()

debug = True if arg.debug > 0 else False
parser = PonyParser(debug=debug, debuglevel=arg.debug)
ast = parser.parse(arg.filename)

if ast == None:
  print('Failed to parse ' + arg.filename)
  sys.exit(-1)

if arg.show:
  ast.show()
  print('\n')

typer = PonyTyper()
typer.typecheck(ast)
