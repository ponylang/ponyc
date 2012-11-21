trait Cloneable[T]
{
  function# clone( pattern = clone_pattern() )->( pattern.clone( this ) )
  function# clone_pattern()->( Partial[T]( this ) )
}
