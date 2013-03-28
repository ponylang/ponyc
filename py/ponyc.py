#!/usr/bin/env python3

import argparse
from program import Program

args = argparse.ArgumentParser()
args.add_argument('-p', '--project', help='Pony project URL', default='.')
args.add_argument('-d', '--debug', help='Debug level', type=int, default=0)
args.add_argument('-o', '--optimize', help='Optimize the lexer and parser',
  type=int, default=0)
args.add_argument('-s', '--show', help='Show AST', action="store_true")
arg = args.parse_args()

program = Program(arg.debug)
program.add_package(arg.project)
program.typecheck()

"""
FIX:
typecheck types
typecheck bodies
generate code
"""
