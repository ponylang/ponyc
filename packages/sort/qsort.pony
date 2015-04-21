type QsortId[A] is QsortFunction[A, OrderIdLt[A]]

type QsortIdReverse[A] is QsortFunction[A, OrderIdGt[A]]

type Qsort[A: Ordered[A] box] is QsortFunction[A, OrderLt[A]]

type QsortReverse[A: Ordered[A] box] is QsortFunction[A, OrderGt[A]]

primitive QsortFunction[A, O: OrderFunction[A] val]
