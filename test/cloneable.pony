trait Constructable[T]
{
  private function construct_new() throws {}
}

class Partial[T]
{
  function fields()->( r:Set[FieldID[T]] )

  function field( f:FieldID[T] )->( r:FieldType[T, f] )

  function setfield( f:FieldID[T], v:FieldType[T, f] )

  function fieldof( f:String )->( r:FieldID[T]|None )
}

trait Cloneable[T]
{
  function clone( p:Partial = clone_pattern() )->( r:T ) throws
  {
    match p
    {
      case as pattern:Partial[T] { ... }
      case { throw; }
    }
  }

  function clone( p:\T = clone_pattern() )->( r:T ) throws
  {
    var q = reflect();

    for f in p.fields()
    {
      var pv:FieldType[T, f] = p.field( f );
      var qv:FieldType[T, f] = q.field( f );

      match v
      {
        case None {}

        // FIX: need to type check v as \Tf
        case as pattern:\FieldType[T, f]
        {
          q.setfield( f, qv.clone( pattern ) );
        }

        case { q.setfield( f, qv.clone() ); }
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
        case as pv:Partial { q.setfield( f, q.field( f ).deserialise( s, pv ) ); }
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
