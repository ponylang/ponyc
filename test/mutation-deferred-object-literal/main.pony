actor \nodoc\ Main
  new create(env: Env) =>
    env.out.print(_Holder.mk().get().string())
