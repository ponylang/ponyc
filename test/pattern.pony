object Partial[T]
{
  // var a:A|Partial[A]|Care|DontCare
  // var b:B|Partial[B]|Care|DontCare
  // var func:lambda( args... )->( results... )|None

  // var _fields:Map[String, Any]
  // var _types:Map[String, Partial!]
  // var _methods:Map[String, Any]

  new( from:T#|None = None )
  {
    match from
    {
      case None { /* all fields are DontCare, all methods are None */ }
      case as v:T# { /* all fields are Care, all methods are captured */ }
    }
  }

  function# typename()->( r:String )
  /*
    r = "T"
  */

  function# field( name:String )->( value:Any? ) throws
  /*
    value = _fields( name )
  */

  function setfield( name:String, value:Any ) throws
  /*
    // FIX: doesn't work well if the field is an ADT
    if value ~= _types( name ) { throw }
    _fields( name ) = value
  */

  function# fields()->( r:Iterator[Pair[String, Any?]] )
  /*
    r = _fields.iterator()
  */

  // FIX: how to express a partial ADT?
  // maybe it just works: Partial[A|B|C]
  function# typeof( name:String )->( value:Partial! ) throws
  /*
    r = _types( name )
  */

  function# typesof()->( r:Iterator[Pair[String, Any?]] )
  /*
    r = _types.iterator()
  */

  function# method( name:String )->( value:Any ) throws
  /*
    value = _methods( name )
  */

  function set_method( name:String, value:Any ) throws
  /*
    // FIX: does it make any sense to set a method? we have no access to a receiver
    // throws if no name, or if type of value doesn't match method signature
    value = _methods( name )
  */

  function# methods()->( r:Iterator[Pair[String, Any?]] )
  /*
    r = _methods.iterator()
  */

  function# equals( a:T#, b:T# )->( r:Bool = false )
  {
    // should this only compare partial to concrete? not two concretes?
    var ra = reflect( a ) // special
    var rb = reflect( b ) // special

    // should we be iterating over object fields instead? if we are
    // a Partial[Trait] or Partial[A|B|C], we have no fields, but we could
    // still determine structural equality

    for name, value in fields()
    {
      match value
      {
        case DontCare {}

        case Care
        {
          if !ra( name ).equals( rb( name ) ) { return }
        }

        case as p:Partial
        {
          if !p.equals( ra( name ), rb( name ) ) { return }
        }

        case // a field type
        {
          if !value.equals( rb( name ) ) { return }
        }
      }
    }

    r = true
  }

  function# equals( a:T# )->( r:Bool = false )
  {
    var mirror = reflect( a ) // special

    for name, value in fields()
    {
      match value
      {
        case DontCare {}

        case as p:Partial
        {
          if !p.equals( mirror( name ) ) { return }
        }

        case
        {
          // if value is a partial object, it's called directly
          // if it is a full object, this will use value.equality_pattern()
          if !value.equals( mirror( name ) ) { return }
        }
      }
    }
  }

  function# clone[U:T]( a:U )->( r:T )
  /*
    match a
    {
      case as v:T! { r = a }

      case as v:T
      {
        r = <ALLOC T>

        for name, value in fields()
        {
          match value
          {
            case Undefined { r.name = a.name } // FIX: unique fields? what's the capability of r?
            case as p:Partial { r.name = p.clone( a.name ) }
            case { r.name = a.name.clone() }
          }
        }
      }
    }
  */

  function# construct()->( r:T? )
  /*
    r = alloc( T ) // special

    for name, value in fields()
    {
      match value
      {
        case Care { r.name = _types( name ).construct() }
        case as p:Partial { r.name = p.construct() }
        case { r.name = value }
      }
    }
  */

  function# reflect[U:T]( a:U )->( r:Partial[U] )
  /*
    r = reflect( a ) // special

    for name, value in fields()
    {
      // leave the value
      case Care {}

      // set the value from the pattern
      // could be a real value, a partial value, or DontCare
      case { r.setfield( name, value ) }
    }

    // leave all the methods
  */
}
