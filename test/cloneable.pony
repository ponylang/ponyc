class Partial[T]
{
  function fields()->( r:Set[Field[T]] )

  function field( f:Field[T] )->( r:FieldType[f] )

  function setfield( f:Field[T], v:FieldType[f] )
}

// one instance per field in T
class Field[T] is Stringable
{
  ambient new( s:String ) throws
}

type FieldType[Field[T]] : (type of field Field[T] of T)

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
      var pv:FieldType[f] = p.field( f );
      var qv:FieldType[f] = q.field( f );

      match v
      {
        case None {}

        case as pattern:\FieldType[f]
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
  function from_string( s:TokenStream, seen = Map[String, Any], p:\T = deserialise_pattern() )
    ->( r:T ) throws
  {
    var q = reflect();
    if s.next() != "{" { throw; }

    var tok = s.next();

    while tok != "}"
    {
      var f = Field[T]( tok );

      if s.next() != ":" { throw; }

      /*
      get a type from the next string
      see if that type is a subtype of f
      */
    }

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

  function from_packed( s:String, p:\T = deserialise_pattern() )

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

trait Constructable[T]
{
  private function construct_new() throws {}
}
