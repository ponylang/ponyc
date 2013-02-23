trait Constructable[T]
{
  private function construct_new() throws {}
}

trait Cloneable[T]
{
  function clone( p:\T = clone_pattern() )->( r:T ) throws
  {
    var q = reflect();

    for f, v in p.fields()
    {
      match v
      {
        case None {}
        // FIX: need to type check v as \Tf
        case as pv:Partial { q.setfield( f, q.field( f ).clone( pv ) ); }
        case { q.setfield( f, q.field( f ).clone() ); }
      }
    }

    r = q.absorb();
    r.clone_new();
  }

  private function clone_new() throws { construct_new(); }

  private function clone_pattern()->( r:\T )
  {
    r = reflect();

    for f, v in r.fields()
    {
      match v
      {
        case Cloneable {}
        case { r.setfield( f, None ); }
      }
    }
  }
}

trait Deserialisable[T]
{
  function deserialise( s:Stream, p:\T = deserialise_pattern() )->( r:T ) throws
  {
    var q = reflect();

    for f, v in p.fields()
    {
      match v
      {
        case None {}
        // FIX: need to type check v as \Tf
        case as pv:Partial { q.setfield( f, q.field( f ).deserialise( s, v ) ); }
        case { q.setfield( f, q.field( f ).deserialise( s ) ); }
      }
    }

    r = q.absorb();
    r.deserialise_new();
  }

  private function deserialise_new() throws { construct_new(); }

  private function deserialise_pattern()->( r:\T )
  {
    r = reflect();

    for f, v in r.fields()
    {
      match v
      {
        case Deserialisable {}
        case { r.setfield( f, None ); }
      }
    }
  }
}
