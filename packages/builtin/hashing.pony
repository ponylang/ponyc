primitive Hashing

  fun hash_bytes(bytes: ByteSeq box): USize =>
    (
      ifdef ilp32 then
        HalfSipHash24[ByteSeq box](bytes)
      else
        SipHash24[ByteSeq box](bytes)
      end
    ).usize()

  fun hash64_bytes(bytes: ByteSeq box): U64 =>
    SipHash24[ByteSeq box](bytes)

  fun hash_array[T: Hashable #read](arr: Array[T] box): USize =>
    iftype T <: U8 then
      hash_bytes(arr)
    else
      ifdef ilp32
        let sip = HalfSipHash24Streaming.create()
        for elem in arr.values() do
          sip.update(elem.hash().u32())
        end
        sip.finish()
      else
        let sip = SipHash24Streaming.create()
        for elem in arr.values() do
          sip.update(elem.hash().u64())
        end
        sip.finish()
      end
    end

  fun hash64_array[T: Hashable64 #read](arr: Array[T] box): U64 =>
    iftype T <: U8 then
      hash64_bytes(arr)
    else
      let sip = SipHash24Streaming.create()
      for elem in arr.values() do
        sip.update(elem.hash64())
      end
      sip.finish()
    end

  fun hash_2(a: Hashable box, b: Hashable box): USize =>
    ifdef ilp32 then
      let sip = HalfSipHash24Streaming.create()
      sip.update(a.hash().u32())
      sip.update(b.hash().u32())
      sip.finish()
    else
      let sip = SipHash24Streaming.create()
      sip.update(a.hash().u64())
      sip.update(b.hash().u64())
      sip.finish()
    end

  fun hash_3(a: Hashable box, b: Hashable box, c: Hashable box): USize =>
    ifdef ilp32 then
      let sip = HalfSipHash24Streaming.create()
      sip.update(a.hash().u32())
      sip.update(b.hash().u32())
      sip.update(c.hash().u32())
      sip.finish()
    else
      let sip = SipHash24Streaming.create()
      sip.update(a.hash().u64())
      sip.update(b.hash().u64())
      sip.update(c.hash().u64())
      sip.finish()
    end

  fun hash_4(a: Hashable box, b: Hashable box, c: Hashable box, d: Hashable box): USize =>
    ifdef ilp32 then
      let sip = HalfSipHash24Streaming.create()
      sip.update(a.hash().u32())
      sip.update(b.hash().u32())
      sip.update(c.hash().u32())
      sip.update(d.hash().u32())
      sip.finish()
    else
      let sip = SipHash24Streaming.create()
      sip.update(a.hash().u64())
      sip.update(b.hash().u64())
      sip.update(c.hash().u64())
      sip.update(d.hash().u64())
      sip.finish()
    end

  // TODO: add functions for up to 16 arguments

  fun hash64_2(a: Hashable64 box, b: Hashable64 box): U64 =>
    let sip = SipHash24Streaming.create()
    sip.update(a.hash64())
    sip.update(b.hash64())
    sip.finish()

  fun hash64_3(a: Hashable64 box, b: Hashable64 box, c: Hashable64 box): U64 =>
    let sip = SipHash24Streaming.create()
    sip.update(a.hash64())
    sip.update(b.hash64())
    sip.update(c.hash64())
    sip.finish()

  fun hash64_4(a: Hashable64 box, b: Hashable64 box, c: Hashable64 box, d: Hashable64 box): U64 =>
    let sip = SipHash24Streaming.create()
    sip.update(a.hash64())
    sip.update(b.hash64())
    sip.update(c.hash64())
    sip.update(d.hash64())
    sip.finish()

  // TODO: add functions for up to 16 arguments
