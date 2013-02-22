object Partial[T]
{
  // var a:A|Partial[A]|DontCare
  // var b:B|Partial[B]|DontCare
  // var func:lambda( args... )->( results... )|None

  // var _fields:Map[String, Any]
  // var _methods:Map[String, Any]

  new()
  /*
  all fields are DontCare, all methods are None
  */

  function typename()->( r:String )
  /*
    r = "T"
  */

  function field( name:String )->( value:Any[:this] ) throws
  /*
    value = _fields( name )
  */

  function~ setfield( name:String, value:Any ) throws
  /*
    // FIX: doesn't work well if the field is an ADT
    if value ~= _types( name ) { throw }
    _fields( name ) = value
  */

  function fields()->( r:Iterator[Pair[String, Any[:this]]] )
  /*
    r = _fields.iterator()
  */

  function method( name:String )->( value:Any ) throws
  /*
    value = _methods( name )
  */

  function~ setmethod( name:String, value:Any ) throws
  /*
    // FIX: does it make any sense to set a method? we have no access to a receiver
    // throws if no name, or if type of value doesn't match method signature
    value = _methods( name )
  */

  function methods()->( r:Iterator[Pair[String, Any[:this]]] )
  /*
    r = _methods.iterator()
  */
}
