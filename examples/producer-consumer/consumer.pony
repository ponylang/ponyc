use "collections"

actor Consumer
  var _quantity_to_consume: U32
  let _buffer: Buffer
  let _out: StdStream

  new create(quantity_to_consume: U32, buffer: Buffer, out: StdStream) =>
    _quantity_to_consume = quantity_to_consume
    _buffer = buffer
    _out = out

  be start_consuming(count: U32 = 0) =>
    _buffer.permission_to_consume(this)
    if count < _quantity_to_consume then start_consuming(count + 1) end

  be consuming(product: Product) =>
    _out.print("**Consumer** Consuming product " + product.id.string())
    _quantity_to_consume = _quantity_to_consume -1
