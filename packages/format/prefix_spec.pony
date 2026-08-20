trait val PrefixSpec
  """
  Base trait for all prefix specifiers.
  """

primitive PrefixDefault is PrefixSpec
  """
  No prefix before the formatted value.
  """

primitive PrefixSpace is PrefixSpec
  """
  Use a space as the prefix for positive numbers.
  """

primitive PrefixSign is PrefixSpec
  """
  Use a `+` sign as the prefix for positive numbers.
  """

type PrefixNumber is
  ( PrefixDefault
  | PrefixSpace
  | PrefixSign )
