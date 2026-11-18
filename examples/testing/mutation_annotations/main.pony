class \nomutate\ Ledger
  """
  A fixed balance and a check for whether a charge would leave it positive.
  """
  fun balance(): U64 =>
    """
    Return the current balance.
    """
    100

  fun \mutate_only(aor, ror)\ affordable(balance': U64, charge: U64): Bool =>
    """
    Return whether the balance would stay positive after the charge.
    """
    (balance'.i128() - charge.i128()) > 0

actor \nomutate\ Main
  new create(env: Env) =>
    """
    Print the balance and whether it can cover a charge of 25.
    """
    let ledger = Ledger
    let balance = ledger.balance()
    env.out.print(balance.string())
    env.out.print(ledger.affordable(balance, 25).string())
