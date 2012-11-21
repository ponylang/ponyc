trait Equality[T]
{
  function# equals( that:T#, pattern = equality_pattern() )->( pattern.equals( this, that ) )
  function# equality_pattern()->( Partial[T]( this ) )
}
