use "collections"

actor Consumer
  var _quantity_to_consume: U32
  let _buffer: Buffer
  let _env: Env

  new create(quantity_to_consume: U32, buffer: Buffer, env: Env) =>
    _quantity_to_consume = quantity_to_consume
    _buffer = buffer
    _env = env
	
  be start_consuming() =>
    for i in Range[U32](0, _quantity_to_consume) do
      _buffer.permission_to_consume(this)
    end

  be consuming(product: Product) =>
    _env.out.print("**Consumer** Consuming product " + product.id.string())
    _quantity_to_consume = _quantity_to_consume -1
		

