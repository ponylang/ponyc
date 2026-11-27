use p1 = "../p1"
primitive _H2
  fun name(): String => "p2._H2"
primitive MakeH2
  fun apply(): HasH2 => _H2
type HasH2 is _H2
trait T2 is p1.T1[_H2]
