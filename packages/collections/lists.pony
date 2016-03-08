primitive Lists
  """
  Helper functions for List. All functions are non-destructive.
  """
  fun flatten[A](l: List[List[A]]): List[A] =>
    """
    Builds a new list out of the elements of the lists in this one.
    """
    let resList = List[A]
    for subList in l.values() do
      resList.append_list(subList)
    end
    resList
