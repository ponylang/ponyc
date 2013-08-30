trait Reflectable[T]
{
  function# reflect( pattern = reflect_pattern() )->( pattern.reflect( this ) )
  function# reflect_pattern()->( Partial[T]( this ) )
}
