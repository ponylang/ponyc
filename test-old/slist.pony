trait List[T] is Iterable[T]
{
  new( item:T, next:List[T]|None = None )

  function# item()->( T? )
  function set_item( T )

  function# next()->( List?[T] )
  function set_next( List[T] )
}

object SListIter[T] is Iterator[T]
{
  var _i:SList#[T]|None

  new( _i )

  function# has_next()->( r:Bool )
  {
    match _i
    {
      case None { r = false }
      case as i:SList#[T] { r = true }
    }
  }

  function next()->( r:T ) throws // FIX: want possibility of a T that isn't readonly
  {
    match _i
    {
      case None { throw }
      case as i:SList#[T]
      {
        r = i.item()
        _i = i.next()
      }
    }
  }
}

object SList[T] is List[T]
{
  var _item:T
  var _next:SList[T]|None

  new( _item, _next = None )

  function# item()->( _item )
  function set_item( _item )

  function# next()->( _next )
  function set_next( _next )

  function# iterator()->( SListIter[T]( this ) )
}

object DList[T] is List[T]
{
  var _item:T
  var _prev:DList[T]|None
  var _next:DList[T]|None

  new( _item, _prev = None, _next = None )
  {
    match _prev
    {
      case None {}
      case as p:DList[T]
      {
        match p.next()
        {
          case None {}
          case as pn:DList[T] { pn.set_prev( None ) }
        }

        p.set_next( this )
      }
    }

    match _next
    {
      case None {}
      case as n:DList[T]
      {
        match n.prev()
        {
          case None {}
          case as np:DList[T] { np.set_next( None ) }
        }

        n.set_prev( this )
      }
    }
  }

  function# item()->( _item )
  function set_item( _item )

  function# next()->( _next )
  function set_next( n:DList[T]|None )
  {
    match _next
    {
      case None {}
      case as nn:DList[T] { nn.set_prev( None ) }
    }

    match n
    {
      case None {}
      case as nn:DList[T] { nn.set_prev( this ) }
    }

    _next = n
  }

  function# prev()->( _prev )
  function set_prev( p:DList[T]|None )
  {
    match _prev
    {
      case None {}
      case as pp:DList[T] { pp.set_next( None ) }
    }

    match p
    {
      case None {}
      case as pp:DList[T] { pp.set_next( this ) }
    }

    _prev = p
  }
}
