trait Cloneable[T]
{
  function# clone( pattern = clone_pattern() )->( r:T )
  {
    r = pattern.clone( this )
  }

  function# clone_pattern()->( r:\T )
  {
    r = reflect()

    for name, value in r.fields()
    {
      match value
      {
        case as v:Cloneable { r.setfield( name, v.clone_pattern() ) }
        case { r.setfield( name, Undefined ) }
      }
    }
  }
}
