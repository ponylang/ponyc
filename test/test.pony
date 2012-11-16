/* Concepts
types
  actors
  objects
  traits
  partial objects
  adts
  parametric types
  mutable, immutable, readonly, unique
local type inference
delegation
exceptions
expression lists
pattern matching
lambdas
  partial application
*/

/* TODO
array literals? other object literals?
  [ arg_list ]

  a = Array[T]( [a, b, c] )
  a = Vector[T, 3]( [a, b, c] )
  a = List[T]( [a, b, c] )
  a = Set[T]( [a, b, c] )
  a = Matrix[T, 3, 3]( [[a, b, c], [d, e, f], [g, h, i]] )
  a = Map[T, U]( [a = b, c = d, e = f] )

access specifiers?
  leading _ for private? what does private even mean?
closures?
  just have partial application?
yield? continuations?
set builder notation, list comprehensions?
  generate lists, sets, maps?
map, fold, zip?

unique receiver
  immutable parameters
    not a problem, they can't hold a reference to self
  unique parameters
    if we pass them self, we are passing them a logically mutable type
    which means we must consume them
    which means we can't return them as a unique
    which means they stay inside our encapsulation boundary

what are the restrictions on value parameters to a gadt?
  must be determinable at compile-time
  then all types are instantiated at compile-time

if immutable functions don't allow 'this' to be sent in a message...
  and don't allow mutable fields to be sent in messages...
  then there is no need for readonly functions or readonly references
  still need a result type that depends on the receiver type?

imports: name them? dot instead of quote?
  importing a directory gets all types in all modules in the directory?
  then a directory is really a package
  need to prevent circular imports?
*/

// use a package or a module
// if its a package, getting at a module type requires a qualified type name
// if its a module, types are unqualified
use Alpha = "some/package"
use Beta = "/some/other/package-2.1"
use Gamma = "http://some.other/package-3.12.4-alpha.1"

// have to use a module to get its declarations?
// that's awkward: if we use package Foo, and type Foo:Bar:Baz is inside module Bar, and Baz relies on some other type
// being declared to use a trait, we don't get that declaration
// can all declarations be used no matter what, no need to import the module?
declare Hoopy::Frood[Argle[Bargle]] is Iggle::Piggle, Upsy[Daisy]
{
  tigger = pooh,
  kanga = roo
}

type Bear:Pooh|Paddington|None

trait Animal is Vegetable, Mineral
{
  function eat( a:Food )->( b:Result, c:Error ) throws
  function# name()->( r:String )
  function! sendto( r:AnimalReceiver )
}

trait AnimalReceiver
{
  message receive( a:Animal! )
}

actor Woodchipper is AnimalReceiver
{
  var _heap = Heap[Animal!]

  message receive( a:Animal! )
  {
    _heap.insert( a )
  }
}

object Aardvark[F:Food, D:Drink] is Animal
{
  var _size = U32( 10 )
  var _t:U32 = 20
  var _bar:Wombat

  delegate _bar:Wombat

  new( _size = 0, _t = _size + 50 )
  {
    _bar = Wombat( "fred" )
  }

  function eat( a:Food )->( b:Result, c:Error|None )
  {
    var q = "q"
    var c = None
    var m:Wombat|Devil, var s = Wombat.make( "bob" ), 99.0e12
    var a, var b = (10).string(), (11.1e-10).string()

    /* this is a /* nested */ comment */
    // this is a line comment
    a, b = _bar.call( (_t + _size).string().initial() ).string()

    match a, m
    {
      case \Termite, \Wombat
      {
        _t = _t + 1
        b = Result( "nice" )
      }
      case \Ant as ant, \Any if ant.size() <= _size
      {
        _t = _t + 10
        b = Result( "happy" )
      }
      case
      {
        b = Result( "sad" )
        c = Error( 10 )
      }
    }
  }

  function dot[T:Number, d:U32]( a:Vector#[T, d], b:Vector#[T, d] )->( r = Vector@[T, d] )
  {
    for i in Range[U32]( 0, d - 1 )
    {
      r( i ) = a( i ) * b( i )
    }
  }

  function dot3( a:Vector#[F32, 3], b:Vector#[F32, 3] )->( dot( a, b ) )
  function# name()->( name )

  function! sendto( a:AnimalReceiver )
  {
    a.receive( this )
  }

  function defaultargs( a:U32 = 20, b = "foo" )
  {
    something( first = "this", second = "that" )
  }
}

actor Console
{
  ambient()

  message print( a:String )
}

trait Named
{
  function# name()->( r:String )
}

type Term:Var|Fun|App is Named

object Var is Named
{
  var _name:String

  new( _name )
  function# name()->( _name )
}

object Fun is Named
{
  var _arg:String
  var _body:Term

  new( _arg, _body )
  function# name()->( "^" + _arg + "." + _body.string() )

  function# identity()->( r = false )
  {
    match _body
    {
      case \Var as v { r = _arg == v.name() }
      case {}
    }
  }
}

object App is Named
{
  var _f:Term
  var _v:Term

  new( _f, _v )
  function# name()->( "(" + _f.string() + " " + _v.string() + ")" )
}

object TermTest
{
  function print( term:Term )
  {
    Console.print( term.name() )
  }

  function identity( term:Term )->( r = false )
  {
    match term
    {
      case \Fun as f
      {
        r = f.identity()
      }
      case {}
    }
  }
}

// tree
trait Tree[T]
{
  function# item()->( r:T# )
  function# depth()->( r:U32 )
}

object Node[T] is Tree[T]
{
  var _left:Tree[T]
  var _item:T
  var _right:Tree[T]

  new( _left, _item, _right )
  function# left()->( _left )
  function# item()->( _item )
  function# right()->( _right )
  function# depth()->( 1 + _left.depth().max( _right.depth() ) )
}

object Leaf[T] is Tree[T]
{
  var _item:T

  new( _item )
  function# item()->( _item )
  function# depth()->( U32( 1 ) )
}

object TreeUtil[U32]
{
  function# sum( t:Tree#[U32] )->( i:U32 )
  {
    match t
    {
      case \Node#[U32] as n
      {
        match n.left(), n.right()
        {
          case \Leaf#[U32] as l, \Leaf#[U32] as r
          {
            i = l.item() + n.item() + r.item()
          }

          case \Leaf#[U32] as l, \Tree#[U32] as r
          {
            i = l.item() + n.item() + sum( r )
          }

          case \Tree#[U32] as l, \Leaf#[U32] as r
          {
            i = sum( l ) + n.item() + r.item()
          }

          case \Tree#[U32] as l, \Tree#[U32] as r
          {
            i = sum( l ) + n.item() + sum( r )
          }
        }
      }

      case \Leaf#[U32] as n
      {
        i = n.item()
      }
    }
  }
}

// intersection types
trait Clone
{
  function# clone()->( r:Clone# )
}

trait Reset
{
  function reset()
}

trait CloneReset is Clone, Reset {}

object Foo
{
  function cloneAndReset( a:CloneReset )->( r:CloneReset )
  {
    r = a.clone()
    a.reset()
  }
}

object Log
{
  var _fd:File

  function log( list:List#[String|lambda()->( String )] )
  {
    for s in list
    {
      match s
      {
        case \String as str { _fd.writeln( str ) }
        case \lambda()->( String ) as f { _fd.writeln( f() ) }
      }
    }
  }
}

object SomeLog
{
  var _log:Log

  function log( i:U32 )
  {
    _log.log(
      [
        \i.string(),
        lambda()->( i.string() + "foo" )
      ]
    )
  }
}

// iterators
trait Iterator[T]
{
  function has_next()->( r:Bool )
  function next()->( r:T ) throws
}

trait Iterable[T]
{
  function# iterator()->( r:Iterator[T] )
}

// ranges
object RangeIter[T:Number] is Iterator[T]
{
  var _min:T
  var _max:T
  var _i:T

  new( _min, _max ) { _i = min }
  function has_next()->( _i <= _max )

  function next()->( r:T ) throws
  {
    if _i <= _max
    {
      r = _i
      _i = _i + 1
    } else {
      throw
    }
  }
}

// any type with no members is a singleton type
object Range[T:Number] is Iterable[T]
{
  var _min:T
  var _max:T

  new( _min, _max )
  function# iterator()->( RangeIter[T]( _min, _max ) )
}

// a fixed size (d) array of T
object Vector[T, d:U64]
{
  // init with default constructor if T has one
  new[T:Default]()

  // init to a
  new with( a:T )

  // init from a function
  new from( f:lambda( n:U64 )->( r:T ) )

  function#( n:U64 )->( r:T )
  function update( n:U64, o:T )
}

// list iterator
object SListIter[T] is Iterator[T]
{
  var _i:SList#[T]|None

  new( _i )

  function# has_next()->( r:Bool )
  {
    match _i
    {
      case \SList#[T] { r = true }
      case None { r = false }
    }
  }

  function next()->( r:T ) throws
  {
    match _i
    {
      case \SList#[T] as l { r = l.first() _i = l.rest() }
      case None { throw }
    }
  }
}

actor Main
{
  message begin( args:Array[String], env:Map[String, String], term:Console|None )
}
