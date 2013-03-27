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

trait Comparable[T]
{
  def eq_with( that:T|\T, pattern:\T ) Bool =
    { this == that } or {
    match that
      | as that:T = that.eq_with( reflect, pattern )
      | as that:\T = this.eq_fields( that, pattern, 0 ) }

  def lt_with( that:T|\T, pattern:\T ) Bool =
    { this == that } or {
    match that
      | as that:T = that.lt_with( reflect, pattern )
      | as that:\T = this.lt_fields( that, pattern, 0 ) }

  def ne_with( a:T|\T, p:\T ) Bool = not this.eq_with( a, p )

  def gt_with( a:T|\T, p:\T ) Bool =
    not this.eq_with( a, p ) and not lt_with( a, p )

  def le_with( a:T|\T, p:\T ) Bool =
    this.eq_with( a, p ) or this.lt_with( a, p )

  def ge_with( a:T|\T, p:\T ) Bool = not this.lt_with( a, p )

  def eq( a:T|\T ) Bool = this.eq_with( a, this.compare_pattern() )
  def lt( a:T|\T ) Bool = this.lt_with( a, this.compare_pattern() )
  def ne( a:T|\T ) Bool = not this.eq( a )
  def gt( a:T|\T ) Bool = not this.eq( a ) and not this.lt( a )
  def le( a:T|\T ) Bool = this.eq( a ) or this.lt( a )
  def ge( a:T|\T ) Bool = not this.lt( a )

  private def eq_fields( that:\T, pattern:\T, i:Number ) Bool =
    match field( pattern, i )
      | None = true
      | as f:FieldID[T] = {
        match that.f
          | None = true
          | = {
            match pattern.f
              | None = true
              | as fpattern:\FieldType[T, f] =
                this.f.eq_with( that.f, fpattern )
              | = this.f.eq( that.f ) } }
        and this.eq_field( i + 1, that, pattern )

  private def lt_fields( that:\T, pattern:\T, i:Number ) Bool =
    match field( pattern, i )
      | None = true
      | as f:FieldID[T] = {
        match that.f
          | None = true
          | = {
            match pattern.f
              | None = true
              | as fpattern:\FieldType[T, f] =
                this.f.lt_with( that.f, fpattern )
              | = this.f.lt( that.f ) } }
        and this.lt_fields( that, pattern, i + 1 )

  private def compare_pattern() \T = reflect
}

trait Constructable[T]
{
  private def construct() T|None = this
}

trait Cloneable[T] is Constructable[T]
{
  def clone() T|None = this.clone_with( this.clone_pattern() )

  // FIX: cycles
  def clone_with( pattern:\T ) T|None =
    match absorb( this.clone_fields( reflect, pattern, 0 ) )
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
      and this.clone_fields( of, pattern, i + 1 )

  private def clone_new() T|None = construct()

  private def clone_pattern() \T = reflect
}

trait Hashable[T]
{
  // FIX: cycles
  def hash( h:Number ) Number = this.hash_with( h, this.hash_pattern() )

  def hash_with( h:Number, pattern:\T ) Number =
    this.hash_fields( h, pattern, 0 )

  private def hash_fields( h:Number, pattern:\T, i:Number ) Number =
    match field( pattern, i )
      | None = h
      | as f:FieldID[T] = this.hash_fields( {
        match this.f
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
  def test() Bool = this.test_with( this.test_pattern() )

  def test_with( pattern:\T ) Bool =
    this.test_methods( reflect, pattern, 0 )

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
      and this.test_methods( mirror, pattern, i + 1 )

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
    this.mother.valid_mother( this ) and this.father.valid_father( this )

  def valid_mother( a:Person ) Bool =
    this.female == true and this.lt_with( a, this.compare_age() )

  def valid_father( a:Person ) Bool =
    this.female == false and this.lt_with( a, this.compare_age() )
}
