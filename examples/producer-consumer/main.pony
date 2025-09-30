actor Main
  """
  Producer-Consumer concurrency problem.

  Pony has no blocking operations.
  The Pony standard library is structured in this way to use notifier objects,
  callbacks and promises to make programming in this style easier.
  """

  new create(env: Env) =>
    let buffer = Buffer(2, env.out)

    let producer = Producer(10, buffer, env.out)
    let consumer = Consumer(10, buffer, env.out)

    consumer.start_consuming()
    producer.start_producing()

    env.out.print("**Main** Finished.")

