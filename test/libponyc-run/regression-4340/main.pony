use "pony_test"

interface Tree
  fun get_value(): USize
  fun univals(): USize
  fun print_values(env: Env)

class Leaf is Tree
  let _value: USize

  new create(value: USize) =>
    _value = value

  fun print_values(env: Env) =>
    None

  fun get_value(): USize => _value

  fun univals(): USize => 1

class Node
  let _value: USize
  let _left: Tree
  let _right: Tree

  new create(value: USize, left: Tree, right: Tree) =>
    _value = value
    _left  = left
    _right = right

  fun get_value(): USize => _value

  fun univals(): USize =>
    let left_univals  = _left.univals()
    let right_univals = _right.univals()
    if (_left.get_value() == _right.get_value())
       and (_left.get_value() == this.get_value()) then
      1 + left_univals + right_univals
    else
      left_univals + right_univals
    end

  fun print_values(env: Env) =>
    env.out.print(this.univals().string())

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_TestUnivals)

class iso _TestUnivals is UnitTest
  fun name(): String => "univals"

  fun apply(h: TestHelper) =>
    h.assert_eq[USize](3, Node(0, Leaf(0), Leaf(0)).univals())
