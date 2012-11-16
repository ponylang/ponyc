trait Pattern[T]
{
  // var _fields:Map[String, Any]

  new( a = Undefined, b = Undefined, ... )
  new all( a = \A, b = \B, ... )

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
    _fields( name ) = value
  */

  function# fields()->( r:Iterator[Pair[String, Any?]] )
  /*
    r = _fields.iterator()
  */

  // what about methods?

  function# equals( a:Any, b:Any )->( r:Bool = false )
  /*
    if a isn't T | b isn't T { return }

    for name, value in fields()
    {
      match value
      {
        case Undefined {}

        case Pattern as p:Pattern
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
}
