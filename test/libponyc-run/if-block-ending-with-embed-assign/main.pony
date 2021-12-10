"""
From Issue #3669
"""

class B
class A
  embed embedded: B
  new create() =>
    if true
    then embedded = B
    else embedded = B
    end

actor Main
  new create(env: Env) =>
    A
