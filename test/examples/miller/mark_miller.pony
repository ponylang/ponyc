/*use "../collections"*/

/* A mint is used as a marker object to indicate that purses share a common
 * currency. The tag capability indicates that when the type has no capability
 * specified, the capability is tag. If no default capability is specified, the
 * default is ref.
 */
class Mint tag

/* A purse contains currency from a specific mint. */
class Purse
  let mint: Mint // let means mint is a single assignment field
  var _balance: U64 // the leading underscore means _balance is private

  // Empty purses can be created freely
  new create(mint': Mint) =>
    mint = mint'
    _balance = 0

  // Inflating the currency requires a Mint ref
  // Note that purses only have a Mint tag
  new inflate(mint': Mint ref, balance: U64) =>
    mint = mint'
    _balance = balance

  // The balance can be read by anyone, but not set
  fun box balance(): U64 => _balance

  // The ? indicates this is a partial function, ie this can have an undefined
  // result - like throwing an exception, but with no value thrown.
  fun ref deposit(amount: U64, from: Purse) ? =>
    // make sure there's enough in 'from' and _balance doesn't overflow
    if (mint == from.mint) and
      (from._balance >= amount) and
      ((_balance + amount) >= _balance)
    then
      _balance = _balance + amount
      from._balance = from._balance - amount
    else
      error
    end

  // Note that this returns an ephemeral isolated Purse
  // The ^ on the end indicates that a type is ephemeral
  fun ref withdraw(amount: U64): Purse iso^ ? =>
    if _balance >= amount then
      // recover is used to get an isolated Purse
      var to = recover Purse(mint) end
      _balance = _balance - amount
      to._balance = amount
      // consume is used to get an ephemeral Purse
      consume to
    else
      // we don't have enough
      error
    end

  // This empties the purse into a new ephemeral isolated purse
  fun ref remainder(): Purse iso^ =>
    var to = recover Purse(mint) end
    _balance = _balance - amount
    to._balance = amount
    consume to

// Things that can be bought
trait Good

/* Notification function
 * This is a structural type. Anything with an apply function of the given
 * signature is this type.
 */
interface Notify tag
  fun tag apply(a: (Good iso | None))

// A buyer has a purse and can be told to buy goods from a seller
actor Buyer
  var _purse: Purse

  new create(purse: Purse) =>
    _purse = purse

  /* This asynchronously attempts to buy a good with a given description from
   * a given seller at a given price. The good, if any, is passed to the
   * supplied result function.
   */
  be buy(desc: String, price: U64, seller: Seller, f: Notify) =>
    try
      seller.sell(desc, _purse.withdraw(price), f)
    else
      f(None)
    end

/* A seller has a purse and an inventory. It can be asked to sell goods with
 * a given description.
 */
interface Factory tag
  fun tag apply(): Good^

type Inventory is Map[String, (U64, Factory)]

actor Seller
  var _purse: Purse
  var _inventory: Inventory

  // The seller should also have a mechanism for populating its inventory.
  new create(purse: Purse) =>
    _purse = purse
    _inventory = Inventory

  /* This asynchronously attempts to buy a good with a given description. A
   * purse containing the payment is supplied along with a result function to
   * receive the good if the purchase is successful.
   */
  be sell(desc: String, payment: Purse, f: Notify) =>
    try
      var (price, factory) = _inventory(desc)
      _purse.deposit(price, payment)
      f(recover factory() end)
    else
      f(None)
    end

/* An asynchronous exchange escrow. It is created empty. Each side submits an
 * Exchange indicating what it provides and what it expects. After submitting,
 * a participant can attempt to cancel - this only works if the other
 * participant has not yet submitted.
 */
actor ExchangeEscrow
  var _first: (Exchange | None)

  new create() =>
    _first = None

  // ex' is shown as a ref here, but since this is an asynchronous method (a
  // behaviour), it must be a sendable subtype of ref at the call-site, ie iso.
  be submit(ex': Exchange) =>
    match _first
    | None =>
      _first = ex' // store this, wait for another submission

    | var ex: Exchange =>
      _first = None // reset the stored submission to None

      try
        ex.expect.deposit(ex.amount, ex'.provide)

        try
          ex'.expect.deposit(ex'.amount, ex.provide)
        else
          ex'.provide.deposit(ex.amount, ex.expect)
          error
        end

        ex.participant.succeed(ex.expect.remainder(), ex.provide.remainder())
        ex'.participant.succeed(ex'.expect.remainder(), ex'.provide.remainder())
      else
        // fail, send back the provided purses
        ex.participant.fail(ex.provide.remainder())
        ex'.participant.fail(ex'.provide.remainder())
      end
    end

  // Note that ex' is tag. This is used to prove that the cancelling party
  // has a reference to the Exchange side being cancelled.
  be cancel(ex': Exchange tag) =>
    match _first
    | var ex: Exchange =>
      if ex is ex' then
        _first = None
        ex.participant.fail(ex.provide.remainder())
      end
    end

/* An Exchange is one side in an ExchangeEscrow contract. A side is composed
 * of a participant, a purse provided to fulfil their end of the bargain, an
 * expected amount, and an expected mint for the amount.
 */
class Exchange
  var participant: Participant
  var provide: Purse
  var expect: Purse
  var amount: U64

  // If both purses are isolated at the call site, this can be recovered as
  // an isolated Exchange.
  new create(participant': Participant, provide': Purse, expect': Purse,
    amount': U64) =>
    participant = participant'
    provide = provide'
    expect = expect'
    amount = amount'

/* A Participant in an Exchange can be informed of success or failure. In the
 * case of success, the participant is sent a purse containing what was
 * expected and a purse containing any change. In the case of failure, the
 * participant is sent back the purse it provided.
 */
trait Participant tag
  be succeed(result: Purse, change: Purse)
  be fail(provided: Purse)

// A marker type.
class Token tag

trait Contract[A]
  fun tag participants(): U64
  fun ref run(args: Array[A])

trait Initiator tag
  be tokens(t: List[Token iso] iso)

// can pop iso elements out of lists relatively easily
actor ContractHost[A, B: Contract[A]]
  let _contract: B
  var _ledger: Array[(Token, (A | None))] = Array[(Token, (A | None))]

  // store our contract, generate a token ledger, and send the isolated array
  // of isolated tokens to the initiator. The initiator can then hand out
  // isolated tokens to participants.
  new create(initiator: Initiator, contract: B) =>
    _contract = contract
    _ledger.reserve(_contract.participants())
    var tokens = recover Array[Token iso] end
    tokens.reserve(_contract.participants())

    for i in Range(0, _contract.participants()) do
      var t = recover Token end
      _ledger.append((t, None))
      tokens.append(consume t)
    end

    // send the isolated array to the initiator
    initiator.tokens(consume tokens)

  // associate a contract side A with a token
  // note this requires an isolated token and so can only be done once per token
  // an index is reported so that the submitter knows which contract side they
  // are signing up for
  // when all tokens are satisfied, the contract is executed
  be submit(side: U64, t: Token iso, arg: A) =>
    try
      if _ledger(side).1 is t then
        _ledger(side) = (t, arg)
        var a = Array[A]
        for (t', arg') in _ledger.values() do
          match arg
          | var arg'': A => a.append(arg'')
          else
            return None
          end
        end
        _contract.run(a)
      end
    end

  // cancel a contract that has not yet been run
  // knowledge of any token is sufficient to cancel a contract
  // however, if the possessor of the isolated token for the side hasn't
  // submitted yet, they may still do so, overriding this cancel
  be cancel(side: U64, t: Token) =>
    // by setting the arg for this token to None, the contract will never run
    try
      if _ledger(side).1 is t then
        _ledger(side) = (t, None)
      end
    end
