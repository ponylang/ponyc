interface _THierKindIface
  fun f(): None

struct _THierKindStruct
  let _f: U32 = 0

actor _THierKindActor is _THierKindIface
  fun f(): None => None

primitive _THierKindPrim is _THierKindIface
  fun f(): None => None
