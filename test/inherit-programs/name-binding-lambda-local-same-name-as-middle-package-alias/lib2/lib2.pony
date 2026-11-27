use u = "collections"
use l1 = "../lib1"
trait T2 is l1.T1
  fun other(): USize => u.Map[String, U8].size()
