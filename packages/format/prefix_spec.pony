trait val PrefixSpec

primitive PrefixDefault is PrefixSpec

primitive PrefixSpace is PrefixSpec
primitive PrefixSign is PrefixSpec

type PrefixNumber is
  ( PrefixDefault
  | PrefixSpace
  | PrefixSign )
