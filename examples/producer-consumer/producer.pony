use "collections"

actor Producer
  var _quantity_to_produce: U32
  let _buffer: Buffer
  var _num: U32 = 0
  let _out: StdStream

  new create(quantity_to_produce: U32, buffer: Buffer, out: StdStream) =>
    _quantity_to_produce = quantity_to_produce
    _buffer = buffer
    _out = out

  be start_producing() =>
    for i in Range[U32](0, _quantity_to_produce) do
      _buffer.permission_to_produce(this)
    end

  be produce() =>
    _out.print("**Producer** Producing product " + _num.string())
    let prod: Product = Product(_num, "Description of product " + _num.string())
    _buffer.store_product(prod)
    _num = _num + 1
