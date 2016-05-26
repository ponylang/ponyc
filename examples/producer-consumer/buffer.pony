actor Buffer
	let capacity: USize
	var _products: Array[Product]
	var _producersWaiting: Array[Producer] = Array[Producer]
	var _consumersWaiting: Array[Consumer] = Array[Consumer]
	let _env: Env
	
	new create(capacity': USize, env: Env) =>
		capacity = capacity'
		_products = Array[Product](capacity)
		_env = env
	
	be permission_to_consume(cons: Consumer) =>
		_env.out.print("**Buffer** Permission_to_consume")
		try
			_env.out.print("**Buffer** Permission_to_consume: Calling consumer to consume")
			cons.consuming(_products.delete(0)) // Fails if products is empty.
			try
				_env.out.print("**Buffer** Permission_to_consume: Calling producer to produce")
				_producersWaiting.delete(0).produce()
			end // Si no hay productores en espera, no hagas nada.
		else
			_env.out.print("**Buffer** Permission_to_consume: Storing consumer in waiting")
			_consumersWaiting.push(cons)
		end
	
	be permission_to_produce(prod: Producer) =>
		_env.out.print("**Buffer** Permission_to_produce")
		if _products.size() < capacity then
			_env.out.print("**Buffer** Permission_to_produce: Calling producer to produce")
			prod.produce()
		else
			_producersWaiting.push(prod)
			_env.out.print("**Buffer** Permission_to_produce: Storing producer in waiting")
		end
	
	be store_product(product: Product) =>
		_env.out.print("**Buffer** Store_product")
		try
			_env.out.print("**Buffer** Store_product: Calling consumer to consume")
			_consumersWaiting.delete(0).consuming(product)
		else
			_env.out.print("**Buffer** Store_product: Storing product")
			_products.push(product)
		end
