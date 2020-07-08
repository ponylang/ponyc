"""
Showcase for defining "value classes" or "data classes": immutable classes
whose value is defined by their immutable fields. Those classes can easily
define `Hashable` and `Equatable`.

In order to implement Hashable, it is sufficient to implement `hash_with`
to update and return the given hash state. Use one of the functions in the
`Hashing` primitive in the builtin package or,
if the fields to consider for hashing are itself Hashable, use their `hash_with`
method and feed the given `HashState` through each call (See `Wombat` class below).
"""

use "collections"


actor Main
  new create(env: Env) =>
    let p1 = Person("Sylvan", BirthDate(1970, 1, 1), Wombat("Bob", 1000))
    let p2 = Person("Darach", BirthDate(2020, 1, 1), Wombat("Mike", 123))
    let p1' = Person("Sylvan", BirthDate(1970, 1, 1), Wombat("Bob", 1000))


    env.out.print("eq: " + (p1 == p2).string())
    env.out.print("eq: " + (p1 == p1').string())
    let counters = Map[Person, U64]
    try
      counters(p1) = (p1.favorite_wombat as Wombat).hunger * (1/3)
      counters(p2) = (p2.favorite_wombat as Wombat).hunger / 5
      counters(p1') = 42
    end
    for (k,v) in counters.pairs() do
      env.out.print(k.string() + " : " + v.string())
    end


class val Person is (Hashable & Hashable64 & Equatable[Person] & Stringable )
  let name: String
  let birth_date: BirthDate
  let favorite_wombat: (Wombat | None)

  new val create(name': String, birth_date': BirthDate, favorite_wombat': (Wombat | None) = None) =>
    name = name'
    birth_date = birth_date'
    favorite_wombat = favorite_wombat'

  fun hash_with(state: HashState): HashState =>
    Hashing.hash_3(
      name,
      birth_date,
      favorite_wombat,
      state
    )

  fun hash_with64(state: HashState64): HashState64 =>
    Hashing.hash64_3(name, birth_date, favorite_wombat, state)

  fun eq(that: box->Person): Bool =>
    (name == that.name) and
    (birth_date == that.birth_date) and
    (
      ((favorite_wombat is None) and (that.favorite_wombat is None))
      or
      try
        (favorite_wombat as Wombat) == (that.favorite_wombat as Wombat)
      else
        false
      end
    )

  fun string(): String iso^ =>
    (recover String end)
      .>append("Person(")
      .>append("name = ")
      .>append(name)
      .>append(", ")
      .>append("birth_date = ")
      .>append(birth_date.string())
      .>append(", ")
      .>append("favorite_wombat = ")
      .>append(
        try (favorite_wombat as Wombat).string() else "None" end)
      .>append(")")


class val Wombat is (Hashable & Hashable64 & Equatable[Wombat] & Stringable)
  let name: String
  let hunger: U64

  new val create(name': String, hunger': U64 = 10) =>
    name = name'
    hunger = hunger'

  fun hash_with(state: HashState): HashState =>
    hunger.hash_with(
      name.hash_with(state)
    )

  fun hash_with64(state: HashState64): HashState64 =>
    hunger.hash_with64(
      name.hash_with64(state)
    )

  fun eq(that: box->Wombat): Bool =>
    (name == that.name) and
    (hunger == that.hunger)

  fun string(): String iso^ =>
    (recover String end)
      .>append("Wombat(")
      .>append("name = ")
      .>append(name)
      .>append(", ")
      .>append("hunger = ")
      .>append(hunger.string())
      .>append(")")


class val BirthDate is (Hashable & Hashable64 & Comparable[BirthDate] & Stringable)
  let year: U16
  let month: U8
  let day: U8

  new val create(year': U16, month': U8, day': U8) =>
    year = year'
    month = month'
    day = day'

  fun hash_with(state: HashState): HashState =>
    Hashing.hash_3(year, month, day, state)

  fun hash_with64(state: HashState64): HashState64 =>
    Hashing.hash64_3(year, month, day, state)

  fun eq(that: box->BirthDate): Bool =>
    (year == that.year) and
    (month == that.month) and
    (day == that.day)

  fun lt(that: box->BirthDate): Bool =>
    (year < that.year) or
    ((year == year) and (month < that.month)) or
    ((year == year) and (month == month) and (day < that.day))

  fun string(): String iso^ =>
    (recover String end)
      .>append("BirthDate(")
      .>append("year = ")
      .>append(year.string())
      .>append(", ")
      .>append("month = ")
      .>append(month.string())
      .>append(", ")
      .>append("day = ")
      .>append(day.string())
      .>append(")")
