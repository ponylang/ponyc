// The runtime rejects a --ponymemoryprofile outside 1 to 10; the argument in
// program-args.txt aborts at startup before Main runs.
actor Main
  new create(env: Env) => None
