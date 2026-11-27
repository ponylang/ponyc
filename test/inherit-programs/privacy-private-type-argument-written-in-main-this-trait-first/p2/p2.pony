use p1 = "../p1"

primitive _H
  fun name(): String => "p2 private H"

type PubH is _H

trait T2 is p1.T1[PubH]

primitive Make
  fun h(): PubH => _H
