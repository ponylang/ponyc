
actor Main
	new create(env: Env) =>
		env.out.print("Hello")
		exp(env)
		foo(env, 5)
		env.out.print("Done")

	fun exp(env: Env) =>
		env.out.print("In exp")

	fun foo(env: Env, exp: ISize) =>
		env.out.print("In foo(" + exp.string() + ")")
	
	fun bar(env: Env) =>
		let exp: ISize = 1
		env.out.print("In bar: exp=" + exp.string())
