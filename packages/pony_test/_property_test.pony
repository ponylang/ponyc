class iso _PropertyTest[T] is UnitTest
  """
  Wraps a Property for use as a PonyTest UnitTest.
  """
  var _prop: (Property[T] iso | None)
  let _name: String

  new iso create(p: Property[T] iso, name': (String | None) = None) =>
    _name =
      match \exhaustive\ name'
      | None => p.name()
      | let s: String => s
      end
    _prop = consume p

  fun name(): String => _name

  fun ref apply(h: TestHelper) ? =>
    """
    Run the wrapped property test.
    """
    let prop = ((_prop = None) as Property[T] iso^)
    let params = prop.params()
    let exec =
      recover iso
        _PropertyExec[T](consume prop, params, h, h.env)
      end
    h._start_property(consume exec)
