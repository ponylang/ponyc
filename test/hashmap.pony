trait Hashable
{
  function hash()->( U64 )
}

object Deleted {}

object Hashmap[K:Hashable!, V]
{
  var _nodes:Array~[Pair~[K, V]|Deleted|None]
  var _count:U64

  new()
  {
    _nodes = Array~[Pair~[K, V]|Deleted|None]
    _count = 0
  }

  function size()->( _count )

  function mod( h:U64 )->( h & (_nodes.length() - 1) )

  function( k:K )->( r:V[:this]|None = None )
  {
    if _count == 0 { return }

    var h = mod( k.hash() )
    var first = h

    do
    {
      // FIX: _nodes( h ) could theoretically throw, but doesn't
      match _nodes( h )
      {
        case None { return }
        case Deleted {}

        case as p:Pair[K, V]
        {
          if k.equals( p.first() )
          {
            r = p.second()
            return
          }
        }
      }

      h = mod( h + 1 )
    } while h != first
  }

  function~ update( k:K, v:V|None )->( r:V|None = None )
  {
    var h = mod( k.hash() )
    var first = h
    var deleted:U64|None = None

    do
    {
      // FIX: _nodes( h ) could theoretically throw, but doesn't
      match _nodes( h ), v
      {
        case Deleted, Any { if deleted == None { deleted = h } }

        case None, None { return }

        case None, as vv:V
        {
          _nodes( h ) = Pair~[K, V]( k, vv )
          _count = _count + 1
          return
        }

        case as p:Pair~[K, V], None
        {
          if k.equals( p.first() )
          {
            r = p.second()
            _nodes( h ) = Deleted
            _count = _count - 1
            return
          }
        }

        case as p:Pair~[K, V], as vv:V
        {
          if k.equals( p.first() )
          {
            r = p.second()
            p.set_second( vv )
            return
          }
        }
      }

      h = mod( h + 1 )
    } while h != first

    match deleted, v
    {
      case Any, None { return }

      case as i:U64, as vv:V
      {
        // FIX: _nodes( i ) could theoretically throw, but won't
        _nodes( i ) = Pair~[K, V]( k, vv )
        _count = _count + 1
        return
      }

      case None, as vv:V
      {
      }
    }

    // FIX: resize the array if needed
  }
}
