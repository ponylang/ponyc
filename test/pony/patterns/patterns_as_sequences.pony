/*

/*
Add to operational semantics:

pattern matching

reference/primitive equality
primitive comparisons
primitive arithmetic

traits
private (for capabilities security)
parameterised types
  parameterised FieldID and MethodID
define how classes are composed from parameters and traits

string from id
id from string

field ordering in partial objects
  to do really arbitrary compare/sort

pull pattern methods from the partial object?
  would allow non-default patterns for some fields

\Vector[var T, var d]
  pattern match on a vector
  if it IS a vector, T is what it's a vector of
  and d is the vector size
  these identifiers are being introduced here
  without 'var', the identifiers must already be bound

*/

/*
what about mode?

we can't reflect an isolated object
what happens when we reflect a frozen object?
  all the fields are frozen, but the pattern shouldn't be
  but it's clearly a different type
  and absorbing it will work differently
*/

trait FieldID[A]
class FieldID_CLASS_0 is FieldID[A]
...

trait FieldType[A, B:FieldID[A]]
class FieldType_CLASS_F0:typeof(CLASS.F0) is FieldType[CLASS, F0]
...

type PartialField[A, B:FieldID[A]]:
  (FieldType[A, B]
  |Partial[FieldType[A, B]]
  |NotPresent
  )

class Partial[A]
  var f0:PartialField[A, F0]
  ...

  var f:List[FieldID[A]]

  fun{var|val} apply[B:FieldID[A]](k:B):PartialField[A, B]->A =
    // magic

  fun{var} update[B:FieldID[A]](k:B, v:PartialField[A, B]->A):PartialField[A, B]->A =
    // magic

trait Reflectable
  fun{var|val} reflect[A:FieldID[This]](k:A):FieldType[This, A]->this =
    // magic

  fun{var|val} reflect():Partial[This{this}] =
    var p = Partial[This{this}];
    for k in p do
      p(k) = reflect(k);
    p

trait Comparable
  fun{var|val} eq(that:(This{var|val}|Partial[This{var|val})]):Bool =
    (this == that) or (
      match that
      | as a:This = a.eq(compare_pattern())
      | as a:Partial[This{_}] =
        for k in a do
          match a(k)
          | as a:NotPresent = None
          | = if not reflect(k).eq(a(k)) then return false end
          end;
        true
      end)

  fun{var|val} lt(that:(This{var|val}|Partial[This{var|val})]):Bool =
    (this != that) and (
      match that
      | as a:This = a.lt(compare_pattern())
      | as a:Partial[This{_}] =
        for k in a do
          match a(k)
          | as a:NotPresent = None
          | = if not reflect(k).lt(a(k)) then return false end
          end;
        true
      end)

  fun{var|val} gt(that:(This{var|val}|Partial[This{var|val})]):Bool =
    not lt(that) and not eq(that)

  fun{var|val} le(that:(This{var|val}|Partial[This{var|val})]):Bool =
    not gt(that)

  fun{var|val} ge(that:(This{var|val}|Partial[This{var|val})]):Bool =
    not lt(that)

  fun{var|val} compare_pattern():Partial[This{this}] = reflect()

trait Constructable
  fun{none} construct(a:Partial[This]):Bool = true

trait Cloneable is Constructable
  fun{val} clone():This{val} = this

  fun{iso|var} clone(map:Map[Any, Any] = Map[Any, Any]):This{iso}|None =

  // FIX: cycles
  def clone_with( pattern:\T ) T|None =
    match absorb( clone_fields( reflect, pattern, 0 ) )
      | None = None
      | as x:T = x.clone_new()

  private def clone_fields( of:\T, pattern:\T, i:Number ) \T =
    match field( of, i )
      | None = of
      | as f:FieldID[T] = {
        of.f = match of.f
          | as off:Cloneable[FieldType[T, f]] = {
            match pattern.f
              | None = off
              | as fpattern:\FieldType[T, f] = off.clonewith( fpattern )
              | = off.clone() }
          | = of.f }
      and clone_fields( of, pattern, i + 1 )

  private def clone_new() T|None = construct()

  private def clone_pattern() \T = reflect
}

trait Hashable[T]
{
  // FIX: cycles
  def hash( h:Number ) Number = hash_with( h, hash_pattern() )

  def hash_with( h:Number, pattern:\T ) Number =
    hash_fields( h, pattern, 0 )

  private def hash_fields( h:Number, pattern:\T, i:Number ) Number =
    match field( pattern, i )
      | None = h
      | as f:FieldID[T] = hash_fields( {
        match f
          | as x:Hashable[FieldType[T, f]] = {
            match pattern.f
              | None = h
              | as fpattern:\FieldType[T, f] = x.hash_with( h, fpattern )
              | = x.hash( h ) }
          | = h },
        pattern, i + 1 )

  private def hash_pattern() \T = reflect
}

trait Testable[T]
{
  def test() Bool = test_with( test_pattern() )

  def test_with( pattern:\T ) Bool =
    test_methods( reflect, pattern, 0 )

  private def test_methods( mirror:\T, pattern:\T, i:Number ) Bool =
    match method( pattern, i )
      | None = true
      | as m:MethodID[T] = {
        match pattern.m
          | None = true
          | = {
            match mirror.m
              | as f:()( Bool ) = f()
              | = true } }
      and test_methods( mirror, pattern, i + 1 )

  private def test_pattern() \T = test_pattern_methods( reflect, 0 )

  private def test_pattern_methods( mirror:\T, i:Number ) \T =
    match method( mirror, i )
      | None = mirror
      | as m:MethodID[T] = {
        mirror.m = if string( m ).begins( "test_" ) then mirror.m else None;
        mirror }
}

class Person is Cloneable, Comparable, Testable
{
  var name:String
  var mother:Person
  var father:Person
  var birthday:Number
  var female:Bool

  private def construct() T|None = if test_parents() then this else None

  private def clone_pattern() \Person = \Person( name, birthday, female )

  def compare_name() \Person = \Person( name )

  def compare_age() \Person = \Person( birthday )

  def compare_ancestry() \Person =
    // 'this' isn't the pattern being constructed
    // new \Person( mother = this, father = this )
    var x = \Person;
    x.mother = x;
    x.father = x;
    x

  def test_parents() Bool =
    mother.valid_mother( this ) and father.valid_father( this )

  def valid_mother( a:Person ) Bool =
    female == true and lt_with( a, compare_age() )

  def valid_father( a:Person ) Bool =
    female == false and lt_with( a, compare_age() )
}

*/
