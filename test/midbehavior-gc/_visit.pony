interface \nodoc\ val _Visit
  fun apply(main: Main ref, depth: U64, seed: U64, collecting: Bool)

primitive \nodoc\ _First is _Visit
  fun \noinline\ apply(
    main: Main ref, depth: U64, seed: U64, collecting: Bool)
  =>
    main._descend(depth, seed, collecting)

primitive \nodoc\ _Second is _Visit
  fun \noinline\ apply(
    main: Main ref, depth: U64, seed: U64, collecting: Bool)
  =>
    main._descend(depth, seed, collecting)
