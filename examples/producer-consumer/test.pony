use "pony_test"
use "time"

actor Main is TestList
    new create(env: Env) => PonyTest(env, this)
    new make() => None
    fun tag tests(test: PonyTest) =>
        test(_TestFifoBasic)

class iso _TestFifoBasic is UnitTest

    fun name():String => "fifo/basic"

    // these are available if we need them:
    // fun ref set_up(h: TestHelper val)? =>
    // fun ref tear_down(h: TestHelper val) =>
  
    fun box label(): String val => "fifo"

    fun apply(h: TestHelper) =>
        let out = h.env.out
        out.print("test: fifo/basic apply() called.")

    // limit to 10 seconds
    let sec10: U64 = 10_000_000_000
    h.long_test(sec10)

    // we need an actor to get the message when the
    // first-to-finish consumer/producer is done doing their thing.
    // This is important because only then can we
    // call h.complete(true) to indicate success,
    // and stop the 10 second test timeout.
    //
    // I'm not sure how tests are supposed to
    // work without an actor to get such messages.
    //
    // Until we learn another way, we use the:
    //
    // mf.producerDone(); and
    // mf.consumerDone() behaviors.
    //
    // Hence we create mf below, and launch Producer and
    // Consumer with it so they can notify it when they finish.
    //
    // The above Done behaviors will call h.complete(true) when
    // either the producer or the consumer finishes.
    //
    // Whichever one wants to do less work (calls their Done first)
    // terminates the test.

    var mf = MainFifo(h)

    let numToProduce: I64 = 3 // 1_000_000
    let numToConsume: I64 = numToProduce
    let fifoSize: USize = 2
    let fifo = Fifo(out, fifoSize, h)
    let t0 = Time.now()

    let p = Producer(out, numToProduce, fifo, mf, h)
    let c = Consumer(out, numToConsume, fifo, mf, h, t0)

    c.start()
    p.start()

    // note: TestHelper asserts look like this:
    //h.assert_eq[U32](4, 2 + 2)


