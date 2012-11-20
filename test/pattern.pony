trait Pattern[T]
{
  // var a:A|\A|Undefined|Default
  // var b:B|\B|Undefined|Default
  // var func:lambda( func args )->( func results )|Undefined

  new( /* a = Undefined, b = Undefined, ... */ )
  new all( /* a = \A, b = \B, ... */ )
  new default( /* a = Default, b = Default, ... */ )

  function# typename()->( r:String )
  /*
    r = "T"
  */

  function# field( name:String )->( value:Any? ) throws
  /*
    // throws if no name
    value = _fields( name )
  */

  function setfield( name:String, value:Any ) throws
  /*
    // throws if no name, or if type of value doesn't match field type
    _fields( name ) = value
  */

  function# fields()->( r:Iterator[Pair[String, Any?]] )
  /*
    r = _fields.iterator()
  */

  function# method( name:String )->( value:Any ) throws
  /*
    // throws if no name
    value = _methods( name )
  */

  function set_method( name:String, value:Any ) throws
  /*
    // throws if no name, or if type of value doesn't match method signature
    value = _methods( name )
  */

  function# methods()->( r:Iterator[Pair[String, Any?]] )
  /*
    r = _methods.iterator()
  */

  function# equals( a:T#, b:T# )->( r:Bool = false )
  /*
    for name, value in fields()
    {
      match value
      {
        case Undefined {}

        case as p:Pattern
        {
          if !p.equals( a.name, b.name ) { return }
        }

        case
        {
          if !value.equality_pattern().equals( a, b ) { return }
        }
      }
    }

    r = true
  */

  function# clone( a:T# )->( r:T )
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
            case Undefined { r.name = a.name }
            case as p:Pattern { r.name = p.clone( a.name ) }
            case { r.name = a.name.clone() }
          }
        }
      }
    }
  */
}
