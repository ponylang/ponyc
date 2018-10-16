
class Vertex[A]
  """
  A vertex of a graph
  """
  var _item: (A | None)
  
  new create(item: (A | None)) =>
    _item = consume item
    
  fun apply(): this->A ?  =>
    """
    Return the item, if we have one, otherwise raise an error.
    """
    _item as this->A
    
