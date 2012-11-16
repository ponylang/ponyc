trait Cloneable[T]
{
  new clone( mirror:\T, pattern:\T )
  {
    for name, value in mirror.fields()
    {
      // FIX: can't do this!
      this.name =
    }
  }

  function# clone( pattern = clone_pattern() )->( r:T )
  {
    r = clone( reflect( pattern ), pattern )
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
