/*
a STEQ b

a.equality_pattern().equals( a, b )

This means partial objects can examine the fields of objects.
Does not violate capabilities since users cannot implement partial objects.
*/

trait Equality[T]
{
  function# equals( that:Any, pattern = equality_pattern() )->( r:Bool )
  {
    r = pattern.equals( this, that )
  }

  function# equality_pattern()->( r:\T )
  {
    r = reflect()

    for name, value in r.fields()
    {
      match value
      {
        case as v:Equality { r.setfield( name, v.equality_pattern() ) }
        case { r.setfield( name, Undefined ) }
      }
    }
  }
}
