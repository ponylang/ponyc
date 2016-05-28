actor Main
  """
  Producer-Consumer concurrency problem.

  Pony has no blocking operations.
  The Pony standard library is structured in this way to use notifier objects,
  callbacks and promises to make programming in this style easier.
  """

  new create(env: Env) =>
    let buffer = Buffer(20, env)
    
    let producer = Producer(2, buffer, env)
    let consumer = Consumer(3, buffer, env)
	
    consumer.start_consuming()
    producer.start_producing()

    env.out.print("**Main** Finished.")
