trait Equality[T]
{
  // call on uniques as well? only with "can't capture" annotation
  function equals(
    that:Any,
    pattern:Partial|DontCare|DefaultPattern = equality_pattern()
    )->( r = false )
  {
    match pattern, that
    {
      case as p:\T, as t:T
      {
        // send our captured fields and the pattern to the other object
        // this doesn't leak capabilities because we are sending our fields
        // to the same function we are currently executing, just with a
        // different receiver
        if !t.equals( reflect( p ), p ) { return }
      }

      /*
      We are a T, so matching a Partial[T] is easy.
      What if we want to match a Partial[T|...] ?
      What if we want to match a Partial[Trait] and we are a Trait?
      */

      case as p:\T, as t:\T
      {
        // reflect using the pattern, only fields we care about are populated
        var mirror = reflect( p )

        // check each field in our mirror
        for name, left in mirror.fields()
        {
          var right = t.field( name )

          match left, right
          {
            // if either side is DontCare, the field doesn't influence equality
            case DontCare, _ {}
            case _, DontCare {}

            // if not all types implement Equality, check for it here
            case
            {
              // recursively check fields
              if !left.equals( right, p.field( name ) ) { return }
            }
          }
        }
      }

      // if the pattern is DontCare, this comparison doesn't influence equality
      case DontCare, _ {}

      // if we have the default pattern, recurse but leave off the pattern
      // argument so that a default equality pattern for this type is used
      case DefaultPattern, _
      {
        if !equals( that ) { return }
      }

      // if the pattern is a Partial, but not on T, this comparison cannot be
      // true - neither 'this' nor 'that' can match the pattern
      case { return }
    }

    r = true
  }

  function equality_pattern()->( r:\T@ )
  {
    /*
    This should be able to create a useful "compare everything" pattern for any
    type, without the programmer having to define this function.
    */
    r = \T@

    for name, _ in r.fields()
    {
      r.setfield( name, DefaultPattern )
    }
  }
}
