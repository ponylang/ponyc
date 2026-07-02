actor Main
  new create(env: Env) =>
    // Regression test for ponylang/ponyc#2701 and ponylang/ponyc#2790.
    //
    // `OutStream.writev` takes `ByteSeqIter`, an interface whose
    // `values()` method returns `Iterator[this->ByteSeq box]` where
    // `ByteSeq` is the type alias `(String | Array[U8] val)`. Array
    // literal element-type inference used to crash the compiler when
    // the element type was extracted from that shape, because a
    // `this->` arrow survived inside a union produced by alias
    // expansion and eventually reached code generation.
    env.out.writev([])
    env.out.writev(["foo"; "bar"])
    // Mixed-type array exercising both branches of `ByteSeq`.
    env.out.writev(["foo"; "bar".array(); "baz"])
