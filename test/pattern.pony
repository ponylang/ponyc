object Partial[T]
{
  // var a:A|Partial[A]|DontCare
  // var b:B|Partial[B]|DontCare
  // var func:lambda( args... )->( results... )|None

  // var _fields:Map[String, Any]
  // var _types:Map[String, Partial!]
  // var _methods:Map[String, Any]

  new()
  /*
  all fields are DontCare, all methods are None
  */

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
}
