use "collections"

actor Producer
  var _quantity_to_produce: U32
  let _buffer: Buffer
  var _num: U32 = 0
  let _env: Env

  new create(quantity_to_produce: U32, buffer: Buffer, env: Env) =>
    _quantity_to_produce = quantity_to_produce
    _buffer = buffer
    _env = env

  be start_producing() =>
    for i in Range[U32](0, _quantity_to_produce) do
      _buffer.permission_to_produce(this)
    end

  be produce() =>
    _env.out.print("**Producer** Producing product " + _num.string())
    let prod: Product = Product(_num, "Description of product " + _num.string())
    _buffer.store_product(prod)
    _num = _num + 1
