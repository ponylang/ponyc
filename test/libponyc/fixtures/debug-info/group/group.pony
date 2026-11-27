"""
A trait whose method has a general body and a body for U8.
"""

trait Grouped
  fun g[A: Any val](a: A): U64 =>
    1

  fun g[A: Any val](a: A): U64 iftype A <: U8 val =>
    let b: U64 = 2
    b
