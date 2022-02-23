/*
This example shows a rather complex scenario.

Here we want to generate both a random size that we use as the length of an array that
we create in our property and a random number of operations on random elements
of the array.

We don't want to use another source of randomness for determining a random element
in our property code, as it is better for reproducability of the property
to make the whole execution only depend on the randomness used when drawing samples
from the Generator.

This way, given a certain seed in the `PropertyParams` we can actually reliably reproduce a failed example.
*/
use "collections"
use "pony_check"

class val _OperationOnCollection[T, R = String] is Stringable
  """Represents a certain operation on an element addressed by idx."""
  let idx: USize
  let op: {(T): R} val

  new val create(idx': USize, op': {(T): R} val) =>
    idx = idx'
    op = op'

  fun string(): String iso^ =>
    recover
      String.>append("_OperationOnCollection(" + idx.string() + ")")
    end

class _OperationOnCollectionProperty is Property1[(USize, Array[_OperationOnCollection[String]])]
  fun name(): String => "collections/operation_on_random_collection_elements"

  fun gen(): Generator[(USize, Array[_OperationOnCollection[String]])] =>
    """
    This generator produces:
      * a tuple of the number of elements of a collection to be created in the property code
      * an array of a randomly chosen operation on a random element of the collection

    Therefore, we first create a generator for the collection size,
    then we `flat_map` over this generator, to generate one for an array of operations,
    whose index is randomly chosen from a range of `[0, num_elements)`.

    The first generated value determines the further values, we want to construct a generator
    based on the value of another one. For this kind of construct, we use `Generator.flat_map`.

    We then `flat_map` again over the Generator for the element index (that is bound by `num_elements`)
    to get a generator for an `_OperationOnCollection[String]` which needs the index as a constructor argument.
    The operation generator depends on the value of the randomly chosen element index,
    thus we use `Generator.flat_map` again.
    """
    Generators.usize(2, 100).flat_map[(USize, Array[_OperationOnCollection[String]])](
      {(num_elements: USize) =>
        let elements_generator =
          Generators.array_of[_OperationOnCollection[String]](
            Generators.usize(0, num_elements-1)
              .flat_map[_OperationOnCollection[String]]({(element) =>
                Generators.one_of[_OperationOnCollection[String]](
                  [
                    _OperationOnCollection[String](element, {(s) => recover String.>append(s).>append("foo") end })
                    _OperationOnCollection[String](element, {(s) => recover String.>append(s).>append("bar") end })
                  ]
                )
              })
            )
        Generators.zip2[USize, Array[_OperationOnCollection[String]]](
          Generators.unit[USize](num_elements),
          elements_generator)
      })

  fun ref property(sample: (USize, Array[_OperationOnCollection[String]]), h: PropertyHelper) =>
    (let len, let ops) = sample

    // create and fill the array
    let coll = Array[String].create(len)
    for i in Range(0, len) do
      coll.push(i.string())
    end

    // execute random operations on random elements of the array
    for op in ops.values() do
      try
        let elem = coll(op.idx)?
        let res = op.op(elem)
        if not h.assert_true(res.contains("foo") or res.contains("bar")) then return end
      else
        h.fail("illegal access")
      end
    end

