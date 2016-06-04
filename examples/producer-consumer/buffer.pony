actor Buffer
  let capacity: USize
  var _products: Array[Product]
  var _future_products: USize = 0
  var _producers_waiting: Array[Producer] = Array[Producer]
  var _consumers_waiting: Array[Consumer] = Array[Consumer]
  let _out: StdStream

  new create(capacity': USize, out: StdStream) =>
    capacity = capacity'
    _products = Array[Product](capacity)
    _out = out

  be permission_to_consume(cons: Consumer) =>
    let debug_string = "**Buffer** Permission_to_consume"
    _out.print(debug_string)
    try
      _out.print(debug_string + ": Calling consumer to consume")
      cons.consuming(_products.delete(0)) // Fails if products is empty.
      try
        _out.print(debug_string + ": Calling producer to produce")
        _producers_waiting.delete(0).produce()
      end  // If there are no producers in waiting, do nothing.
    else
      _out.print(debug_string + ": Storing consumer in waiting")
      _consumers_waiting.push(cons)
    end

  be permission_to_produce(prod: Producer) =>
    let debug_string = "**Buffer** Permission_to_produce"
    _out.print(debug_string)
    if (_products.size() + _future_products) < capacity then
      _future_products = _future_products + 1
      _out.print(debug_string + ": Calling producer to produce")
      prod.produce()
    else
      _producers_waiting.push(prod)
      _out.print(debug_string + ": Storing producer in waiting")
    end

  be store_product(product: Product) =>
    let debug_string = "**Buffer** Store_product"
    _out.print(debug_string)
    _future_products = _future_products - 1
    if _consumers_waiting.size() > 0 then
      try
       _out.print(debug_string + ": Calling consumer to consume")
        _consumers_waiting.delete(0).consuming(product)
      else
        _out.print(debug_string + ": Storing product")
        _products.push(product)
      end
    end
